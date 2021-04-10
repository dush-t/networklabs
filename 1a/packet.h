#define PAYLOAD_SIZE 100
#define PORT 8010

// Packet types
#define ACK 1
#define DATA 2

typedef struct Packets {
    int size;
    int seq_no;
    short type;
    short last_pkt;
    char payload[PAYLOAD_SIZE];
} Packet;