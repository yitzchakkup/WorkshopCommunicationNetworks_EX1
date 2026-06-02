# Makefile for TCP Benchmarking Application

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic

# Target executables
TARGETS = client server

# Default target
all: $(TARGETS)

# Rule to build the client
client: client.c
	$(CC) $(CFLAGS) -o client client.c

# Rule to build the server
server: server.c
	$(CC) $(CFLAGS) -o server server.c

# Clean up build artifacts
clean:
	rm -f $(TARGETS)
