# Makefile for TCP Benchmarking Application

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic

# Target executables
TARGETS = client server

# Default target
all: $(TARGETS)

# Rule to build the client
client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

# Rule to build the server
server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

# Clean up build artifacts
clean:
	rm -f $(TARGETS)
