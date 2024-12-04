# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread

# Executables
SERVER = server
CLIENT = client

# Default target
all: $(SERVER) $(CLIENT)

# Compile the server
$(SERVER): server.c
	$(CC) $(CFLAGS) -o $(SERVER) server.c

# Compile the client
$(CLIENT): client.c
	$(CC) $(CFLAGS) -o $(CLIENT) client.c

# Clean up build artifacts
clean:
	rm -f $(SERVER) $(CLIENT)

# Run the server
run-server: $(SERVER)
	./$(SERVER)

# Run the client
run-client: $(CLIENT)
	./$(CLIENT)

