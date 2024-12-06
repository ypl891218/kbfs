#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unordered_map>
#include "ff_api.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 512
/* kevent set */
struct kevent kevSet;
/* events */
struct kevent events[MAX_EVENTS];
int kq;
int server_socket;

int proc_id;

void init_child(int id) {
    proc_id = id;
    sleep(5);
    char *procid = (char*)malloc(sizeof(char)*64);
    snprintf(procid, 64, "--proc-id=%d", id);
    char *ff_argv[4] = {
        "./ganesha.nfsd",
        "--conf=/data/f-stack/config.ini",
        "--proc-type=secondary",
    };
    ff_argv[3] = procid;
    ff_init(4, ff_argv);
    printf("Finish init in child %d\n", id);
    fflush(stdout);    
}

void init_parent() {
    proc_id = 0;
    char *ff_argv[4] = {
        "./ganesha.nfsd",
        "--conf=/data/f-stack/config.ini",
        "--proc-type=primary",
        "--proc-id=0"
    };
    ff_init(4, ff_argv);
    printf("Finish init in parent\n");
    fflush(stdout);
}

void prepare_socket() {
    struct sockaddr_in server_address;
    kq = ff_kqueue();

    printf("prepare socket\n");
    fflush(stdout);

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
    if (ff_listen(server_socket, MAX_EVENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);
    fflush(stdout);

    EV_SET(&kevSet, server_socket, EVFILT_READ, EV_ADD, 0, MAX_EVENTS, NULL);
    /* Update kqueue */
    ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);
}

std::unordered_map<int, FILE*> clientFile;

int loop(void *arg) {
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
            ff_close(clientfd);
            clientFile.erase(clientfd);
        } else if (clientfd == server_socket) {
            int available = (int)event.data;
            do {
                int client_socket;
                struct sockaddr_in client_address;
                socklen_t addr_len = sizeof(client_address);

                // Accept a new client
                if ((client_socket = ff_accept(server_socket,
                    (struct linux_sockaddr*)&client_address, &addr_len)) < 0) {
                    perror("Accept failed");
                    continue;
                }

                printf("[Proc %d] accept a client: %d\n", proc_id, client_socket);
                fflush(stdout);
                // Create a thread to handle the client
                EV_SET(&kevSet, client_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
                ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);
                available--;
            } while (available);
        } else if (event.filter == EVFILT_READ) {
            char buffer[BUFFER_SIZE];
            char command[BUFFER_SIZE];
            char filename[BUFFER_SIZE];
            ssize_t bytes_received;

            if ((bytes_received = ff_recv(clientfd, buffer, BUFFER_SIZE, 0)) <= 0) {
                perror("Failed to receive data");
                ff_stop_run();
            }
            buffer[bytes_received] = '\0';

            // Parse request
            printf("bytes_received: %d\n", bytes_received);
            sscanf(buffer, "%s %s", command, filename);
            printf("command = %s, filename = %s\n", command, filename);
            FILE* file = fopen(filename, "r");
            clientFile[clientfd] = file;            

            EV_SET(&kevSet, clientfd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
            ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);
            //ff_close(clientfd);
        } else if (event.filter == EVFILT_WRITE) {
            char buffer[BUFFER_SIZE];
            int bytes_sent;
            while ((bytes_sent = fread(buffer, 1, BUFFER_SIZE, clientFile[clientfd])) > 0) {
                size_t total_sent = 0;
                while (total_sent < bytes_sent) {
                    int result = ff_send(clientfd, buffer + total_sent, bytes_sent - total_sent, MSG_DONTWAIT);
                    if (result < 0) {
                        perror("Send error");
                        break;
                    }
                    total_sent += result;
                }
                if (total_sent < bytes_sent) {
                    break;
                }
            }
            if (bytes_sent == 0) {
                fclose(clientFile[clientfd]);
                ff_close(clientfd);
                clientFile.erase(clientfd);
            } else {
                EV_SET(&kevSet, clientfd, EVFILT_WRITE, EV_ENABLE, 0, 0, NULL);
                ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);
            }
        }
    }
    return 0;
}

int main() {
    pid_t pid;

    printf("Parent process: PID = %d\n", getpid());

    for (int i = 1; i <= 3; i++) {
        pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            printf("Child process %d: PID = %d, Parent PID = %d\n", i + 1, getpid(), getppid());
            init_child(i);
            prepare_socket();
            ff_run(loop, NULL);
            exit(0);
        }
    }

    init_parent();
    prepare_socket();
    ff_run(loop, NULL);

    return 0;
}
