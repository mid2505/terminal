#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

#define PORT 8080
#define WINDOW_SIZE 4
#define TOTAL_FRAMES 8

// Simple packet structure
typedef struct {
    int seq_num;
    char data[50];
} Packet;

// Receiver state
bool received[TOTAL_FRAMES] = {false};
char buffer[TOTAL_FRAMES][50];
int expected = 0;

void send_ack(int client_socket, int seq_num) {
    Packet ack;
    ack.seq_num = seq_num;
    strcpy(ack.data, "ACK");
    
    send(client_socket, &ack, sizeof(Packet), 0);
    printf("Sent ACK for frame %d\n", seq_num);
}

void process_frame(int seq_num, char *data) {
    if (seq_num >= 0 && seq_num < TOTAL_FRAMES && !received[seq_num]) {
        received[seq_num] = true;
        strcpy(buffer[seq_num], data);
        printf("Stored frame %d: \"%s\"\n", seq_num, data);
        
        // Deliver frames in order
        while (expected < TOTAL_FRAMES && received[expected]) {
            printf("Delivered frame %d to application: \"%s\"\n", expected, buffer[expected]);
            expected++;
        }
    } else if (received[seq_num]) {
        printf("Duplicate frame %d received: \"%s\"\n", seq_num, data);
    }
}

void print_status() {
    printf("Receiver Status: ");
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        printf("[%d:%c] ", i, received[i] ? 'Y' : 'N');
    }
    printf("| Next expected: %d\n", expected);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    Packet packet;
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Allow port reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Setup address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind and listen
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1);
    
    printf("=== Simple Selective Repeat Server ===\n");
    printf("Waiting for client on port %d...\n", PORT);
    
    client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    printf("Client connected!\n\n");
    
    while (expected < TOTAL_FRAMES) {
        int bytes = recv(client_socket, &packet, sizeof(Packet), 0);
        
        if (bytes <= 0) {
            printf("Client disconnected\n");
            break;
        }
        
        printf("\n--- Received frame %d: \"%s\" ---\n", packet.seq_num, packet.data);
        
        // Simple packet loss simulation (20% loss)
        if (rand() % 100 < 20) {
            printf("Frame %d LOST (simulated network loss)\n", packet.seq_num);
            continue;
        }
        
        process_frame(packet.seq_num, packet.data);
        send_ack(client_socket, packet.seq_num);
        print_status();
    }
    
    printf("\n🎉 All frames received and delivered!\n");
    printf("Complete message sequence:\n");
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        printf("Frame %d: \"%s\"\n", i, buffer[i]);
    }
    
    close(client_socket);
    close(server_fd);
    return 0;
}
