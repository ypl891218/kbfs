#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 200

typedef struct {
    int client_socket;
    struct sockaddr_in client_address;
} ClientData;

void* handle_client(void* arg) {
    ClientData* client_data = (ClientData*) arg;
    int client_socket = client_data->client_socket;
    free(client_data);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    // Receive client request
    if ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) <= 0) {
        perror("Failed to receive data");
        close(client_socket);
        pthread_exit(NULL);
    }
    buffer[bytes_received] = '\0';

    // Parse request
    char command[10], filename[BUFFER_SIZE];
    sscanf(buffer, "%s %s", command, filename);

    if (strcmp(command, "READ") == 0) {
        FILE* file = fopen(filename, "r");
	printf("%s\n", filename);
	fflush(stdout);
        if (file == NULL) {
            perror("ERROR: File not found");
    	    send(client_socket, "ERROR: File not found\n", 22, 0);
        } else {
            int sz = 0;
            int bytes_sent = 0;
            while ((bytes_sent = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                size_t total_sent = 0;
                while (total_sent < bytes_sent) {
                    int result = send(client_socket, buffer + total_sent, bytes_sent - total_sent, 0);
                    if (result < 0) {
                        perror("Send error");
                        fclose(file);
                    }
                    total_sent += result;
                }
                sz += bytes_sent;
            }

            fclose(file);
        }
    } else if (strcmp(command, "WRITE") == 0) {
        FILE* file = fopen(filename, "w");
        if (file == NULL) {
            send(client_socket, "ERROR: Cannot open file\n", 24, 0);
        } else {
            char* content = buffer + strlen(command) + strlen(filename) + 2;
            fprintf(file, "%s", content);
            fclose(file);
            send(client_socket, "SUCCESS: File written\n", 23, 0);
        }
    } else {
        send(client_socket, "ERROR: Invalid command\n", 23, 0);
    }

    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    pthread_t threads[MAX_CLIENTS];
    int thread_count = 0;

    while (1) {
        ClientData* client_data = malloc(sizeof(ClientData));
        socklen_t addr_len = sizeof(client_data->client_address);

        // Accept a new client
        if ((client_data->client_socket = accept(server_socket, 
            (struct sockaddr*)&client_data->client_address, &addr_len)) < 0) {
            perror("Accept failed");
            free(client_data);
            continue;
        }

        printf("New client connected: %d\n", thread_count);

        // Create a thread to handle the client
        if (pthread_create(&threads[thread_count++], NULL, handle_client, client_data) != 0) {
            perror("Thread creation failed");
            close(client_data->client_socket);
            free(client_data);
            continue;
        }

        // Detach the thread to allow cleanup on completion
        pthread_detach(threads[thread_count - 1]);

        // Limit to 100 clients
        if (thread_count >= MAX_CLIENTS) {
            printf("Maximum clients reached. Refusing new connections.\n");
        }
    }

    close(server_socket);
    return 0;
}

