#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>

#define PORT 8080
#define WINDOW_SIZE 4
#define TOTAL_FRAMES 8
#define TIMEOUT 3

typedef struct {
    int seq_num;
    char data[50];
} Packet;

// Real messages to send
char* messages[TOTAL_FRAMES] = {
    "hi",
    "hello", 
    "how are you?",
    "good morning",
    "nice weather today",
    "see you later",
    "goodbye",
    "take care"
};

// Sender state
bool acked[TOTAL_FRAMES] = {false};
time_t sent_time[TOTAL_FRAMES] = {0};
int base = 0;
int next_seq = 0;

void send_frame(int server_socket, int seq_num) {
    Packet packet;
    packet.seq_num = seq_num;
    strcpy(packet.data, messages[seq_num]);
    
    send(server_socket, &packet, sizeof(Packet), 0);
    sent_time[seq_num] = time(NULL);
    printf("Sent frame %d: \"%s\"\n", seq_num, messages[seq_num]);
}

void print_window() {
    printf("Sender Window [base=%d, next=%d]: ", base, next_seq);
    for (int i = base; i < base + WINDOW_SIZE && i < TOTAL_FRAMES; i++) {
        if (i < next_seq) {
            printf("[%d:%s:%c] ", i, acked[i] ? "ACK" : "SENT", acked[i] ? '✓' : '-');
        } else {
            printf("[%d:READY] ", i);
        }
    }
    printf("\n");
}

bool can_send() {
    return (next_seq < base + WINDOW_SIZE) && (next_seq < TOTAL_FRAMES);
}

void slide_window() {
    while (base < TOTAL_FRAMES && acked[base]) {
        printf("📬 Window slides: base %d -> %d (ACKed: \"%s\")\n", 
               base, base + 1, messages[base]);
        base++;
    }
}

void check_timeouts(int server_socket) {
    time_t now = time(NULL);
    bool timeout_occurred = false;
    
    for (int i = base; i < next_seq; i++) {
        if (!acked[i] && (now - sent_time[i]) >= TIMEOUT) {
            printf("⏰ TIMEOUT: Retransmitting frame %d: \"%s\"\n", i, messages[i]);
            send_frame(server_socket, i);
            timeout_occurred = true;
        }
    }
    
    if (timeout_occurred) {
        print_window();
    }
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    Packet ack_packet;
    
    srand(time(NULL));
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // Setup server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    // Set receive timeout for non-blocking ACK reception
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection failed! Make sure server is running.\n");
        return -1;
    }
    
    printf("=== Simple Selective Repeat Client ===\n");
    printf("Connected to server!\n");
    printf("Sending messages: ");
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        printf("\"%s\"", messages[i]);
        if (i < TOTAL_FRAMES - 1) printf(", ");
    }
    printf("\n\n");
    
    time_t last_check = time(NULL);
    
    while (base < TOTAL_FRAMES) {
        // Send new frames if window allows
        while (can_send()) {
            printf("\n--- Sending new frame ---\n");
            send_frame(sock, next_seq);
            next_seq++;
            print_window();
        }
        
        // Check for ACKs
        int bytes = recv(sock, &ack_packet, sizeof(Packet), 0);
        if (bytes > 0 && strcmp(ack_packet.data, "ACK") == 0) {
            int ack_num = ack_packet.seq_num;
            printf("\n--- ✅ Received ACK for frame %d: \"%s\" ---\n", 
                   ack_num, messages[ack_num]);
            
            if (ack_num >= base && ack_num < next_seq && !acked[ack_num]) {
                acked[ack_num] = true;
                slide_window();
                print_window();
            }
        }
        
        // Check timeouts every 2 seconds
        if (time(NULL) - last_check >= 2) {
            printf("\n--- ⏱️  Checking for timeouts ---\n");
            check_timeouts(sock);
            last_check = time(NULL);
        }
        
        usleep(200000); // 200ms delay
    }
    
    printf("\n🎉 All messages sent and acknowledged!\n");
    printf("Transmission complete!\n");
    
    close(sock);
    return 0;
}
