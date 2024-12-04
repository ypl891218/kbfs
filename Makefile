# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread

PKGCONF ?= pkg-config
CFLAGS_FF = $(CFLAGS) -g -gdwarf-2 $(shell $(PKGCONF) --cflags libdpdk)

# Executables
FFSERVER = ff_server
SERVER = server
CLIENT = client

# Default target
all: $(FFSERVER) $(SERVER) $(CLIENT)

FF_PATH = /data/f-stack
LIBS+= $(shell $(PKGCONF) --static --libs libdpdk)
LIBS+= -L${FF_PATH}/lib -Wl,--whole-archive,-lfstack,--no-whole-archive
LIBS+= -Wl,--no-whole-archive -lrt -lm -ldl -lcrypto -pthread -lnuma

# Compile the ff server
$(FFSERVER): ff_server.c
	$(CC) $(CFLAGS_FF) -o $(FFSERVER) ff_server.c $(LIBS)

# Compile the server
$(SERVER): server.c
	$(CC) $(CFLAGS) -o $(SERVER) server.c

# Compile the client
$(CLIENT): client.c
	$(CC) $(CFLAGS) -o $(CLIENT) client.c

# Clean up build artifacts
clean:
	rm -f $(SERVER) $(CLIENT) $(FFSERVER)

# Run the server
run-server: $(SERVER)
	./$(SERVER)

# Run the client
run-client: $(CLIENT)
	./$(CLIENT)

