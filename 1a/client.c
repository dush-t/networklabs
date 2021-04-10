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

#define RETRANSMISSION_TIMEOUT 2
#define MAXFDS 32

int next_seq_no = 0;
fd_set readfds;

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

void
send_packet(Packet* p, int sock) {
    int attempt = 0;
    while (1) {
        struct timeval timeout;
        timeout.tv_sec = RETRANSMISSION_TIMEOUT;
        if (attempt < 1) {
            printf("SENT PKT: Seq. No. = %d, Size = %d Bytes\n", next_seq_no, PAYLOAD_SIZE);
        }
        else {
            printf("RE-TRANSMIT PKT: Seq. No. = %d, Size = %d Bytes\n", next_seq_no, PAYLOAD_SIZE);
        }

        // Send the data packet
        if (send(sock, (char*)p, sizeof(Packet), 0) < 0) {
            perror("send");
            return;
        }

        // Wait for corresponding ack with a timeout
        FD_ZERO(&readfds); FD_SET(sock, &readfds);
        int numready = select(MAXFDS, &readfds, NULL, NULL, &timeout);
        if (numready < 1) {
            attempt++;
            continue;
        }


        for (int i = 0; i <= MAXFDS; i++) {
            if (FD_ISSET(i, &readfds)) {
                char recvbuf[sizeof(Packet)];
                if (recv(i, &recvbuf, sizeof(Packet), 0) < 0) {
                    perror("recv");
                    return;
                }
                Packet* ack = (Packet*)recvbuf;
                printf("RCVD ACK: Seq. No. = %d\n", ack->seq_no);
                next_seq_no += PAYLOAD_SIZE;
                return;
            }
        }
    }
}

int
main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;

    int conn_stat = connect(sock, (struct sockaddr*)&saddr, sizeof(saddr));
    if (conn_stat < 0) {
        perror("connect");
        return -1;
    }

    FILE* fp = fopen("input.txt", "r");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    while (1) {
        Packet* p = next_packet(fp);
        if (p == NULL) {
            printf("Error generating packet\n");
            return -1;
        }

        sleep(1);
        send_packet(p, sock);

        if (p->last_pkt) {
            printf("Finished file upload.\n");
            break;
        }
    }

    return 0;
}