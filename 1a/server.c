#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "packet.h"

#define REJECT_PROBABILITY 0.5

char result[1024 * 20];
int idx = 0;

void add_to_result(char val[], int size) {
    for (int i = 0; i < size; i++) {
        result[idx++] = val[i];
    }
}

int
handle_packet(int csock) {
    char recvbuff[sizeof(Packet)];
    if (recv(csock, &recvbuff, sizeof(Packet), 0) < 0) {
        perror("recv");
        return -1;
    }
    Packet* pkt = (Packet*)recvbuff;

    // Reject packet with probability = REJECT_PROBABILITY
    if (((double)rand() / (double)RAND_MAX) < REJECT_PROBABILITY) {
        printf("DROP PKT: Seq. No. = %d\n", pkt->seq_no);
        return 0;
    }
    printf("RCVD PKT: Seq. No. = %d, Size = %d Bytes\n", pkt->seq_no, pkt->size);

    // Add packet payload to array
    // printf("pkt: %s | last = %d\n", pkt->payload, pkt->last_pkt);
    // strcat(result, pkt->payload);
    add_to_result(pkt->payload, pkt->size);

    // Send back ACK
    Packet ack;
    ack.type = ACK;
    ack.seq_no = pkt->seq_no;
    if (send(csock, (char*)&ack, sizeof(ack), 0) < 0) {
        perror("send");
        return -1;
    }
    printf("SENT ACK: Seq. No. = %d\n", ack.seq_no);

    if (pkt->last_pkt) {
        return 1;
    }

    return 0;
}

void
write_to_file() {
    FILE* fp = fopen("output.txt", "w");
    if (fp == NULL) {
        perror("fopen");
    }

    result[idx++] = 0;
    printf("%s\n", result);
    int n = fprintf(fp, "%s", result);
    if (n < 0) {
        perror("fprintf");
    }
}

void
handle_connection(int csock) {
    int status;
    while (1) {
        status = handle_packet(csock);
        if (status != 0) {
            break;
        }
    }

    printf("status = %d\n", status);

    if (status < 0) {
        printf("There was an error receiving data!\n");
        return;
    }

    if (status > 0) {
        write_to_file();
    }
    close(csock);
    printf("Connection closed\n");
}


int
main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&saddr, sizeof(saddr));
    if (listen(sock, 5) < 0) {
        perror("listen");
        return -1;
    }
    printf("Server is up on port %d\n", PORT);

    while (1) {
        struct sockaddr_in caddr;
        int addr_len = sizeof(caddr);
        int csock = accept(sock, (struct sockaddr*)&caddr, &addr_len);
        if (csock < 0) {
            perror("accept");
            continue;
        }

        printf("Accepted connection from %s:%d\n", 
            inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

        // if (fork() == 0) {
            handle_connection(csock);
        //     return 0;
        // }

        close(csock);
    }

    return 0;
}