#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void die(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        die("socket");
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        die("setsockopt");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        die("bind");
    }

    if (listen(server_fd, 5) < 0) {
        die("listen");
    }

    printf("Server listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("\nClient connected.\n");

        uint32_t header[2];
        ssize_t bytes_read = 0;
        size_t header_size = sizeof(header);
        char *header_ptr = (char*)header;

        while (bytes_read < (ssize_t)header_size) {
            ssize_t r = recv(client_fd, header_ptr + bytes_read, header_size - bytes_read, 0);
            if (r <= 0) break;
            bytes_read += r;
        }

        if (bytes_read != (ssize_t)header_size) {
            fprintf(stderr, "Failed to read header from client. Closing connection.\n");
            close(client_fd);
            continue;
        }

        uint32_t num_messages = ntohl(header[0]);
        uint32_t message_size = ntohl(header[1]);

        printf("Expecting %u messages of size %u bytes.\n", num_messages, message_size);

        char *buffer = (char*)malloc(message_size);
        if (!buffer) {
            fprintf(stderr, "Failed to allocate buffer\n");
            close(client_fd);
            continue;
        }

        int error = 0;
        for (uint32_t i = 0; i < num_messages; ++i) {
            size_t msg_bytes_read = 0;
            while (msg_bytes_read < message_size) {
                ssize_t r = recv(client_fd, buffer + msg_bytes_read, message_size - msg_bytes_read, 0);
                if (r <= 0) { error = 1; break; }
                msg_bytes_read += r;
            }
            if (error) break;
        }

        if (error) {
            fprintf(stderr, "Error receiving messages or client disconnected early.\n");
            free(buffer);
            close(client_fd);
            continue;
        }

        printf("Successfully received all %u messages.\n", num_messages);

        const char *ack_msg = "ACK";
        size_t ack_len = strlen(ack_msg);
        ssize_t sent = send(client_fd, ack_msg, ack_len, 0);
        if (sent != (ssize_t)ack_len) {
            fprintf(stderr, "Failed to send ACK.\n");
        } else {
            printf("Sent ACK message to client.\n");
        }

        free(buffer);
        close(client_fd);
        printf("Connection closed.\n");
    }

    close(server_fd);
    return 0;
}
