#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Helper function to handle errors
void die(const std::string& msg) {
    perror(msg.c_str());
    exit(1);
}

int main(int argc, char* argv[]) {
    // Usage: ./server <port>
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);

    // Create the server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        die("socket");
    }

    // Set socket options to allow address reuse
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        die("setsockopt");
    }

    // Configure the server address structure
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        die("bind");
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) < 0) {
        die("listen");
    }

    std::cout << "Server listening on port " << port << "...\n";

    // Main server loop
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept an incoming connection
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        std::cout << "\nClient connected.\n";

        // In order to dynamically receive X messages without framing issues in TCP,
        // we establish a simple protocol:
        // 1. Client sends a header with 2 x uint32_t (network byte order):
        //    [number of messages (X)] [size of each message]
        // 2. Client sends X messages of the specified size.
        
        uint32_t header[2];
        ssize_t bytes_read = 0;
        size_t header_size = sizeof(header);
        char* header_ptr = reinterpret_cast<char*>(header);
        
        // Read the header
        while (bytes_read < (ssize_t)header_size) {
            ssize_t r = recv(client_fd, header_ptr + bytes_read, header_size - bytes_read, 0);
            if (r <= 0) {
                break; // Connection closed or error
            }
            bytes_read += r;
        }

        if (bytes_read != (ssize_t)header_size) {
            std::cerr << "Failed to read header from client. Closing connection.\n";
            close(client_fd);
            continue;
        }

        // Decode the header
        uint32_t num_messages = ntohl(header[0]);
        uint32_t message_size = ntohl(header[1]);

        std::cout << "Expecting " << num_messages << " messages of size " << message_size << " bytes.\n";

        // Buffer for receiving a single message
        std::vector<char> buffer(message_size);
        bool error = false;
        
        // Loop to receive the dynamic batch of X messages
        for (uint32_t i = 0; i < num_messages; ++i) {
            size_t msg_bytes_read = 0;
            while (msg_bytes_read < message_size) {
                ssize_t r = recv(client_fd, buffer.data() + msg_bytes_read, message_size - msg_bytes_read, 0);
                if (r <= 0) {
                    error = true;
                    break;
                }
                msg_bytes_read += r;
            }
            if (error) {
                break;
            }
        }

        if (error) {
            std::cerr << "Error receiving messages or client disconnected early.\n";
            close(client_fd);
            continue;
        }

        std::cout << "Successfully received all " << num_messages << " messages.\n";

        // Once all X messages are received, send a single short ACK
        const char* ack_msg = "ACK";
        size_t ack_len = std::strlen(ack_msg);
        ssize_t sent = send(client_fd, ack_msg, ack_len, 0);
        
        if (sent != (ssize_t)ack_len) {
            std::cerr << "Failed to send ACK.\n";
        } else {
            std::cout << "Sent ACK message to client.\n";
        }

        // Close the connection as requested
        close(client_fd);
        std::cout << "Connection closed.\n";
    }

    close(server_fd);
    return 0;
}
