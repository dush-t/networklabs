#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "packet.h"

#define SELECT_TIMEOUT 10
#define MAXFDS 32

int next_seq_no = 0;

int sock1, sock2;
Packet *p1, *p2;
fd_set readfds, writefds;


Packet*
next_packet(FILE* fp) {
    Packet* p = (Packet*)malloc(sizeof(Packet));

    p->type = DATA;
    p->seq_no = next_seq_no;
    p->last_pkt = 0;

    int nread = fread(p->payload, 1, PAYLOAD_SIZE, fp);
    if (nread > 0) {
        p->size = nread;
        if (nread < PAYLOAD_SIZE) {
            printf("last packet\n");
            p->last_pkt = 1;
        }
        return p;
    }
    else {
        perror("fread");
        return NULL;
    }
}

int
send_next_packet(int sock, FILE*fp) {
    Packet* p = next_packet(fp);
    if (p == NULL) {
        close(sock);
    }
    if (send(sock, (char*)p, sizeof(Packet), 0) < 0) {
        perror("sock send"); return -1;
    }
    printf("SENT PKT: Seq. No. = %d, Size = %d Bytes, CH=%d\n", p->seq_no, p->size, sock);
    next_seq_no += PAYLOAD_SIZE;

    if (p->last_pkt) return 1;
    return 0;
}

int
send_file(FILE* fp) {

    if (send_next_packet(sock1, fp) == 1) {
        return 0;
    }
    if (send_next_packet(sock2, fp) == 1) {
        return 0;
    }

    while (1) {
        struct timeval timeout;
        timeout.tv_sec = SELECT_TIMEOUT;

        FD_ZERO(&readfds); FD_ZERO(&writefds);
        FD_SET(sock1, &readfds); FD_SET(sock2, &readfds);
        int numready = select(MAXFDS, &readfds, NULL, NULL, &timeout);
        if (numready < 1) {
            continue;
        }

        for (int i = 0; i < MAXFDS; i++) {
            if (FD_ISSET(i, &readfds)) {
                char recvbuf[sizeof(Packet)];
                if (recv(i, &recvbuf, sizeof(Packet), 0) < 0) {
                    perror("recv"); return -1;
                }
                Packet* ack = (Packet*)recvbuf;
                printf("RCVD ACK: Seq. No. = %d\n", ack->seq_no);
                
                if (i == sock1) {
                    if (send_next_packet(sock1, fp) == 1) {
                        return 0;
                    }
                }
                else if (i == sock2) {
                    if (send_next_packet(sock2, fp) == 1) {
                        return 0;
                    }
                }
            }
        }
    }
}


int
main() {
    sock1 = socket(AF_INET, SOCK_STREAM, 0);
    sock2 = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (connect(sock1, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("sock1 connect"); 
        return -1;
    }

    if (connect(sock2, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("sock2 connect"); 
        return -1;
    }

    FILE* fp = fopen("input.txt", "rb");
    if (fp == NULL) {
        perror("fopen"); 
        return -1;
    }

    send_file(fp);
}