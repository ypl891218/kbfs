# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread

PKGCONF ?= pkg-config
CFLAGS_FF = $(CFLAGS) -g -gdwarf-2 $(shell $(PKGCONF) --cflags libdpdk)

# Executables
NEWSERVER = new_server
FFSERVER = ff_server
SERVER = server
CLIENT = client

# Default target
all: $(FFSERVER) $(SERVER) $(CLIENT) $(NEWSERVER)

FF_PATH = /data/f-stack
LIBS+= $(shell $(PKGCONF) --static --libs libdpdk)
LIBS+= -L${FF_PATH}/lib -Wl,--whole-archive,-lfstack,--no-whole-archive
LIBS+= -Wl,--no-whole-archive -lrt -lm -ldl -lcrypto -pthread -lnuma

$(NEWSERVER): new_server.cpp
	g++ $(CFLAGS_FF) -o $(NEWSERVER) new_server.cpp $(LIBS)

# Compile the ff server
$(FFSERVER): ff_server.c
	$(CC) $(CFLAGS_FF) -o $(FFSERVER) ff_server.c $(LIBS)

# Compile the server
$(SERVER): server.cpp
	g++ $(CFLAGS) -o $(SERVER) server.cpp -lkqueue

# Compile the client
$(CLIENT): client.c
	$(CC) $(CFLAGS) -o $(CLIENT) client.c

# Clean up build artifacts
clean:
	rm -f $(SERVER) $(CLIENT) $(FFSERVER) $(NEWSERVER)

# Run the server
run-server: $(SERVER)
	./$(SERVER)

# Run the client
run-client: $(CLIENT)
	./$(CLIENT)

