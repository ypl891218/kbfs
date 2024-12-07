#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unordered_map>
#include <sys/ioctl.h>
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 512

#include "ff_api.h"

int proc_id;
int server_socket;
std::unordered_map<int, FILE*> clientFile;

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

    printf("Preparing socket\n");
    fflush(stdout);

    // Create socket
    if ((server_socket = ff_socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set non-blocking mode
    int on = 1;
    if (ff_ioctl(server_socket, FIONBIO, &on) < 0) {
        perror("Setting non-blocking mode failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (ff_setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (ff_setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("reuse port");
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
}

struct pollfd fds[MAX_EVENTS];
int nfds;

int loop(void* arg) {
    int ret = ff_poll(fds, nfds, -1);
    if (ret < 0) {
        perror("poll failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nfds; ++i) {
        if (fds[i].revents & POLLIN) {
            if (fds[i].fd == server_socket) {
                // Accept new client
                struct sockaddr_in client_address;
                socklen_t addr_len = sizeof(client_address);
                int client_socket = ff_accept(server_socket, (struct linux_sockaddr*)&client_address, &addr_len);
                if (client_socket < 0) {
                    perror("Accept failed");
                    continue;
                }

                printf("[Proc %d] Accepted new client: %d\n", proc_id, client_socket);
                fflush(stdout);

                // Add client to poll set
                fds[nfds].fd = client_socket;
                fds[nfds].events = POLLIN;
                nfds++;
            } else {
                // Handle client request
                char buffer[BUFFER_SIZE];
                ssize_t bytes_received = ff_recv(fds[i].fd, buffer, BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    continue;
                }

                buffer[bytes_received] = '\0';
                printf("Received: %s\n", buffer);

                // Process request (e.g., file operations)
                char command[BUFFER_SIZE], filename[BUFFER_SIZE];
                sscanf(buffer, "%s %s", command, filename);
                FILE *file = fopen(filename, "r");
                clientFile[fds[i].fd] = file;

                fds[i].events = POLLOUT;
            }
        } else if (fds[i].revents & POLLOUT) {
            // Handle client response
            char buffer[BUFFER_SIZE];
            ssize_t bytes_sent;
            FILE *file = clientFile[fds[i].fd];
            if (!file || (bytes_sent = fread(buffer, 1, BUFFER_SIZE, file)) == 0) {
                fclose(file);
                clientFile.erase(fds[i].fd);
                ff_close(fds[i].fd);
                fds[i] = fds[nfds - 1];
                nfds--;
            } else {
                ff_send(fds[i].fd, buffer, bytes_sent, MSG_DONTWAIT);
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
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            printf("Child process %d: PID = %d, Parent PID = %d\n", i, getpid(), getppid());
            proc_id = i;
            init_child(proc_id);
            prepare_socket();
            nfds = 1;

            fds[0].fd = server_socket;
            fds[0].events = POLLIN;

            ff_run(loop, NULL);

            exit(0);
        }
    }

    init_parent();
    prepare_socket();
    nfds = 1;

    fds[0].fd = server_socket;
    fds[0].events = POLLIN;

    ff_run(loop, NULL);
    return 0;
}

