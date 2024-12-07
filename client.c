#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void send_command(int socket, const char* command, const char* file_path) {
    // Open the file for writing
    FILE* output_file = fopen(file_path, "w");
    if (!output_file) {
        perror("Failed to open file");
        return;
    }

    send(socket, command, strlen(command), 0);

    char buffer[BUFFER_SIZE];
    ssize_t total_received = 0;
    ssize_t bytes_received;

    // Receive server response
    while ((bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        // Write buffer content to the file
        fwrite(buffer, sizeof(char), bytes_received, output_file);
        total_received += bytes_received;
 //       printf("bytes_received %d\n", bytes_received);
    }
    // Close the file
    fclose(output_file);
    close(socket);
}

int main(int argc, char *argv[]) {
    if (argc <= 1) {
       perror("Server IP needed");
       exit(EXIT_FAILURE);
    }
    int client_socket;
    struct sockaddr_in server_address;
 	char input[BUFFER_SIZE];
   	char command[BUFFER_SIZE];
    char mode[128];
    char serverpath[128];
    char filepath[128];

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert and set server IP
    if (inet_pton(AF_INET, argv[1], &server_address.sin_addr) <= 0) {
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

    // Option variables
    int opt;
    char *command_str = NULL;  // Will hold the command passed with -c

    // Parse command-line options
    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
            case 'c':
                command_str = optarg; // Store the command passed via -c
                break;
            default:
                fprintf(stderr, "Usage: %s <server_ip> [-c \"command\"]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (command_str) {
        // Parse the command string (e.g., "READ /path/to/file clientfile")
        if (sscanf(command_str, "%s %s %s", mode, serverpath, filepath) != 3) {
            fprintf(stderr, "Invalid command format. Expected: READ/WRITE <server_path> <client_path>\n");
            exit(EXIT_FAILURE);
        }

        snprintf(command, BUFFER_SIZE, "%s %s", mode, serverpath);
        printf("%s\n", command);
        // Send the command directly
        send_command(client_socket, command, filepath);

        return 0;
    }

    printf("Connected to the server.\n");
	while (1) {
    	printf("\nEnter a command (READ <server_path> <client_path>, WRITE <server_path> <client_path>, or EXIT):\n> ");
    	fgets(input, BUFFER_SIZE, stdin);

    	// Remove trailing newline
    	input[strcspn(input, "\n")] = '\0';

    	if (strcmp(input, "EXIT") == 0) {
        	printf("Exiting client.\n");
        	break;
    	}
        // Parse the input into command (READ <server_path> or WRITE <server_path>) and filepath
        if (sscanf(input, "%s %s %s", mode, serverpath, filepath) != 3) {
            printf("Invalid command format. Please use READ <server_path> <client_path> or WRITE <server_path> <client_path>.\n");
            continue;
        }

        // Combine the command and server_path into a single command string
        snprintf(command, BUFFER_SIZE, "%s %s", mode, serverpath);

    	// Send the command (excluding filepath) and handle the file path separately
    	send_command(client_socket, command, filepath);
	}
    close(client_socket);
    return 0;
}

