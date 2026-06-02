#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>

// Self-contained error-handling function
void die(const char* message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void send_warmup(const char* ip, int port, uint32_t count, uint32_t size) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        die("Socket creation error");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        die("Invalid address/ Address not supported");
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        die("Connection Failed");
    }

    uint32_t header[2] = {htonl(count), htonl(size)};
    if (send(sock, header, sizeof(header), 0) < 0) {
        die("Header send failed");
    }

    std::vector<char> msg_buffer(size, 'a');

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t sent = 0;
        while (sent < size) {
            int n = send(sock, msg_buffer.data() + sent, size - sent, 0);
            if (n < 0) die("Message send failed");
            sent += n;
        }
    }

    char ack_buffer[4] = {0};
    if (recv(sock, ack_buffer, sizeof(ack_buffer), 0) < 0) {
        die("ACK receive failed");
    }

    close(sock);
}


int main(int argc, char const *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <Server IP> <Server Port>\n";
        return -1;
    }

    const char* server_ip = argv[1];
    int port = std::stoi(argv[2]);

    std::cout << std::fixed << std::setprecision(2);

    // Benchmark for message sizes from 1 byte up to 1MB
    for (uint32_t size = 1; size <= 1024 * 1024; size *= 2) {
        // --- Warm-up Phase ---
        // Perform 100 warm-up cycles to stabilize the TCP connection's congestion
        // window and the system's buffers. This helps mitigate initial performance 
        // fluctuations, ensuring representative steady-state performance.
        const uint32_t warmup_cycles = 100;
        send_warmup(server_ip, port, warmup_cycles, size);

        // --- Measurement Phase ---
        // Calculate a dynamic batch size 'X'. For smaller messages, we send a large
        // number to average out per-packet overhead. For larger messages, we send 
        // fewer to keep the test efficient.
        const long long total_data_per_test = 32 * 1024 * 1024; // 32MB
        uint32_t batch_size = total_data_per_test / size;
        if (batch_size < 10) batch_size = 10;
        if (batch_size > 200000) batch_size = 200000;

        int sock = 0;
        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            die("Socket creation error");
        }
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
            die("Invalid address/ Address not supported");
        }
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            die("Connection Failed");
        }

        uint32_t header[2] = {htonl(batch_size), htonl(size)};
        if (send(sock, header, sizeof(header), 0) < 0) {
            die("Header send failed");
        }

        std::vector<char> msg_buffer(size, 'a');

        // Start the timer immediately after the warm-up cycles.
        auto start = std::chrono::high_resolution_clock::now();

        // Send the batch of X messages.
        for (uint32_t i = 0; i < batch_size; ++i) {
            uint32_t sent = 0;
            while (sent < size) {
                int n = send(sock, msg_buffer.data() + sent, size - sent, 0);
                if (n < 0) die("Message send failed");
                sent += n;
            }
        }

        // Wait to recv() the single acknowledgment from the server.
        char ack_buffer[4] = {0};
        if (recv(sock, ack_buffer, sizeof(ack_buffer), 0) < 0) {
            die("ACK receive failed");
        }

        // Stop the timer. (The time waiting for the server's reply can be ignored in the calculation).
        auto end = std::chrono::high_resolution_clock::now();
        close(sock);

        std::chrono::duration<double> elapsed_seconds = end - start;
        double total_megabytes = (static_cast<double>(batch_size) * size) / (1024.0 * 1024.0);
        
        // Calculate the throughput in Megabytes per second (MB/s).
        double throughput_mbps = total_megabytes / elapsed_seconds.count();

        // Print the result strictly using this format: exactly three columns delimited by a single tab (\t).
        std::cout << size << "\t" << throughput_mbps << "\tMB/s\n";
    }

    return 0;
}
