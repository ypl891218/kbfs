#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include "ff_api.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 200
#define MAX_EVENTS 512

/* kevent set */
struct kevent kevSet;
/* events */
struct kevent events[MAX_EVENTS];
char html[] =
"HTTP/1.1 200 OK\r\n"
"Server: F-Stack\r\n"
"Date: Sat, 25 Feb 2017 09:26:33 GMT\r\n"
"Content-Type: text/html\r\n"
"Content-Length: 438\r\n"
"Last-Modified: Tue, 21 Feb 2017 09:44:03 GMT\r\n"
"Connection: keep-alive\r\n"
"Accept-Ranges: bytes\r\n"
"\r\n"
"<!DOCTYPE html>\r\n"
"<html>\r\n"
"<head>\r\n"
"<title>Welcome to F-Stack!</title>\r\n"
"<style>\r\n"
"    body {  \r\n"
"        width: 35em;\r\n"
"        margin: 0 auto; \r\n"
"        font-family: Tahoma, Verdana, Arial, sans-serif;\r\n"
"    }\r\n"
"</style>\r\n"
"</head>\r\n"
"<body>\r\n"
"<h1>Welcome to F-Stack!</h1>\r\n"
"\r\n"
"<p>For online documentation and support please refer to\r\n"
"<a href=\"http://F-Stack.org/\">F-Stack.org</a>.<br/>\r\n"
"\r\n"
"<p><em>Thank you for using F-Stack.</em></p>\r\n"
"</body>\r\n"
"</html>";
typedef struct {
    int id;
    int client_socket;
    struct sockaddr_in client_address;
    int my_kq;
    struct kevent my_kevent;
    char command[10];
    char filename[BUFFER_SIZE];
} ClientData;

int loop_send(void *arg) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
 
    ClientData* data = (ClientData*)arg;

    int my_kq = data->my_kq;
    struct kevent events[2];
    int num_events = ff_kevent(my_kq, NULL, 0, events, 2, NULL);
    int client_socket = data->client_socket;

    if (num_events < 0) {
        perror("Error: kevent");
        return -1;
    }

    for (int i = 0; i < num_events; ++i) {
        printf("num_events = %d\n", num_events);
        if (events[i].filter == EVFILT_READ) {
            // Receive client request
            if ((bytes_received = ff_recv(client_socket, buffer, BUFFER_SIZE, 0)) <= 0) {
                perror("Failed to receive data");
                ff_stop_run();
            }
            buffer[bytes_received] = '\0';

            // Parse request
            printf("bytes_received: %d\n", bytes_received);
            sscanf(buffer, "%s %s", data->command, data->filename);
            printf("command = %s, filename = %s\n", data->command, data->filename);
            FILE* file = fopen(data->filename, "r");
            if (file == NULL) {
                perror("ERROR: File not found");
                ff_stop_run();
            } else {
                int sz = 0;
                int bytes_sent = 0;
                int result = ff_write(client_socket, html, sizeof(html));
                printf("%d\n", result);
/*
                while ((bytes_sent = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                    size_t total_sent = 0;
                    while (total_sent < bytes_sent) {
                        int result = ff_write(client_socket, buffer + total_sent, bytes_sent - total_sent);
                        printf("send result = %d, total = %ld\n", result, sz);
                        if (result < 0) {
                            perror("Send error");
                            fclose(file);
                            ff_stop_run();
                        }
                        total_sent += result;
                    }
                    sz += bytes_sent;
                }
                fclose(file);
*/
                ff_stop_run();
            }
        }
    }
    return -1;
}

void* handle_client(void* arg) {
    ClientData* data = (ClientData*) arg;
    int client_socket = data->client_socket;
    printf("handle_client: %d!\n", client_socket);
    fflush(stdout);

    data->my_kq = ff_kqueue();
    EV_SET(&(data->my_kevent), client_socket, EVFILT_READ | EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, MAX_EVENTS, NULL);
    if (ff_kevent(data->my_kq, &(data->my_kevent), 1, NULL, 0, NULL)) {
        perror("ff_kevent failed");
    }
    data->client_socket = client_socket;

    printf("after private ff_kevent\n");

    ff_run(loop_send, data);

    printf("exit ff_run for client\n");
   
    ff_close(client_socket);
    exit(0);
}

int kq;
int server_socket;

int loop(void *arg) {
    pthread_t threads[MAX_CLIENTS];
    int thread_count = 0;

    int nevents = ff_kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
    if (nevents < 0) {
        printf("ff_kevent failed:%d, %s\n", errno,
                    strerror(errno));
        return -1;
    }

    for (int i = 0; i < nevents; ++i) {
        printf("%d\n", nevents);
        struct kevent event = events[i];
        int clientfd = (int)event.ident;

        if (event.flags & EV_EOF) {
            /* Simply close socket */
            // ff_close(clientfd);
        } else if (clientfd == server_socket) {
            int available = (int)event.data;
            do {
                ClientData* client_data = malloc(sizeof(ClientData));
                socklen_t addr_len = sizeof(client_data->client_address);

                // Accept a new client
                if ((client_data->client_socket = ff_accept(server_socket, 
                    (struct linux_sockaddr*)&client_data->client_address, &addr_len)) < 0) {
                    perror("Accept failed");
                    free(client_data);
                    continue;
                }

                client_data->id = ++thread_count;
                // Create a thread to handle the client
                if (fork() == 0) {
                    handle_client(client_data);
                }
                available--;
            } while (available);
            
            // Limit to 100 clients
            if (thread_count >= MAX_CLIENTS) {
                printf("Maximum clients reached. Refusing new connections.\n");
                break;
            }
        }
    }
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        wait();
    }
    return 0;
}

int main() {
    char *ff_argv[4] = {
        "./ganesha.nfsd",
        "--conf=/data/f-stack/config.ini",
        "--proc-type=primary",
        "--proc-id=0"
    };
    ff_init(4, ff_argv);    

    struct sockaddr_in server_address;
    kq = ff_kqueue();

    // Create socket
    if ((server_socket = ff_socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    /* Set non blocking */
    int on = 1;
    ff_ioctl(server_socket, FIONBIO, &on);

    int opt = 1;
    if (ff_setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    // Bind socket
    if (ff_bind(server_socket, (struct linux_sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (ff_listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    EV_SET(&kevSet, server_socket, EVFILT_READ, EV_ADD, 0, MAX_EVENTS, NULL);
    /* Update kqueue */
    ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);
    ff_run(loop, NULL);


    close(server_socket);
    return 0;
}

