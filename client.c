#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void send_command(int socket, const char* command) {
    send(socket, command, strlen(command), 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    printf("Server Response:\n");

    // Receive server response
    while ((bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        // Exit if less data received indicates end of response
        if (bytes_received < BUFFER_SIZE - 1) {
            break;
        }
    }
    printf("\n");
}

int main() {
    int client_socket;
    struct sockaddr_in server_address;

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert and set server IP
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server.\n");

    while (1) {
        char command[BUFFER_SIZE];
        printf("\nEnter a command (READ <filename>, WRITE <filename> <content>, or EXIT):\n> ");
        fgets(command, BUFFER_SIZE, stdin);

        // Remove trailing newline
        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "EXIT") == 0) {
            printf("Exiting client.\n");
            break;
        }

        send_command(client_socket, command);
    }

    close(client_socket);
    return 0;
}

