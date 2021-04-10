#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "packet.h"

#define SELECT_TIMEOUT 10
#define MAXFDS 33
#define BASE_LENGTH 1024

unsigned char* buffer;
int length = BASE_LENGTH;
int start_index = 0, max_end = 0;
int outputfd;

int csocks[8];
int num_clients = 0;
fd_set readfds;

void
write_to_file(unsigned char* buffer, int length) {
    int written = write(outputfd, buffer, length);
    if (written < 0) {
        perror("write");
    }
}

void
shift_buffer(unsigned char* buffer, int shift) {
    for (int i = shift; i < max_end; i++) {
        buffer[i-shift] = buffer[i];
    }
}

void
add_to_buffer(unsigned char* target, unsigned char* value, int seq, int size) {
    printf("%s\n", value);
    int start = seq - start_index;
    int end = start + size;
    if (end > max_end) max_end = end;
    if (end > length) {
        length *= 2;
        target = (unsigned char*)realloc((void*)target, length);
    }
    for (int i = 0; i < size; i++) {
        target[start+i] = value[i];
    }

    int write_idx = 0;
    for (int i = 0; i < length; i++) {
        if (target[i] == 0) break;
        write_idx++;
    }

    if (write_idx != 0) {
        unsigned char writebuf[write_idx];
        for (int i = 0; i < write_idx; i++) {
            writebuf[i] = target[i];
        }
        write_to_file(writebuf, write_idx);
        shift_buffer(target, write_idx);
        start_index = write_idx;
    }
}

void
handle_packet(Packet* pkt, int idx) {
    sleep(1);
    printf("RCVD PKT: Seq. No. = %d, Size = %d Bytes\n", pkt->seq_no, pkt->size);

    // OUTPUT PACKET TO FILE OR SOMETHING
    add_to_buffer(buffer, pkt->payload, pkt->seq_no, pkt->size);

    int csock = csocks[idx];
    Packet ack;
    ack.type = ACK;
    ack.seq_no = pkt->seq_no;
    if (send(csock, (char*)&ack, sizeof(ack), 0) < 0) {
        perror("ack send"); return;
    }
    printf("SENT ACK: Seq. No. = %d, CH=%d\n", ack.seq_no, csock);
}

int
is_transfer_complete() {
    for (int i = 0; i < num_clients; i++) {
        if (csocks[i]) return 0;
    }
    return 1;
}

int
handle_connections(int sock) {
    
    for (int i = 0; i < 32; i++) csocks[i] = 0;

    int init = 0;
    while (1) {
        if (init && is_transfer_complete()) {
            printf("Transfer complete\n");
            return 0;
        }

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        for (int i = 0; i < num_clients; i++) {
            if (csocks[i]) FD_SET(csocks[i], &readfds);
        }

        int numread = select(MAXFDS, &readfds, NULL, NULL, NULL);
        if (numread < 0) {
            perror("select"); return -1;
        }

        // Listening socket is readable, so new connection
        // is incoming
        if (FD_ISSET(sock, &readfds)) {
            struct sockaddr_in caddr;
            int addr_len = sizeof(caddr);
            int csock = accept(sock, (struct sockaddr*)&caddr, &addr_len);
            if (csock < 0) {
                perror("accept");
                continue;
            }
            printf("Accepted connection from %s:%d\n", 
                inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
            csocks[num_clients++] = csock;
            init = 1;
        }

        for (int i = 0; i < num_clients; i++) {
            int csock = csocks[i];
            if (csock != 0 && FD_ISSET(csock, &readfds)) {
                char recvbuff[sizeof(Packet)];
                int recvstat = recv(csock, &recvbuff, sizeof(Packet), 0);
                if (recvstat < 0) {
                    perror("recv"); return -1;
                }
                else if (recvstat == 0) {
                    printf("Socket %d closed by client\n", csock);
                    csocks[i] = 0;
                    close(csock);
                    FD_CLR(csock, &readfds);
                }
                else {
                    Packet* pkt = (Packet*)recvbuff;
                    handle_packet(pkt, i);
                }
            }
        }
    }
}

int
main() {
    buffer = (unsigned char*)malloc(length);
    bzero(buffer, length);
    outputfd = open("output.txt", O_WRONLY | O_CREAT, 0644);
    if (outputfd < 0) {
        perror("open"); return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&saddr, sizeof(saddr));
    if (listen(sock, 10) < 0) {
        perror("listen"); return -1;
    }

    printf("Server is up on port %d\n", PORT);

    handle_connections(sock);
}