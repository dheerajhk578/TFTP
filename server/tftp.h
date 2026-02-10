/* Common file for server & client*/

#ifndef TFTP_H
#define TFTP_H
 
#include <stdint.h>
#include <arpa/inet.h>

#define PORT        6969
#define BUFFER_SIZE 516   // 2 opcode + 2 block + 512 data
#define TIMEOUT_SEC 5

// IMPORTANT: match client packing to keep structures identical on the wire
#pragma pack(push, 1)

// TFTP OpCodes
typedef enum {
    RRQ = 1,  // Read Request
    WRQ = 2,  // Write Request
    DATA = 3, // Data Packet
    ACK = 4,  // Acknowledgment
    ERROR = 5 // Error Packet
} tftp_opcode;
// TFTP Mode
typedef enum{
    DEFAULT  = 1, // Read 512 bytes
    OCTET    = 2, // Read 1 byte
    NETASCII = 3  // Conversion for linux <-> windows
}tftp_mode;
// TFTP Packet Structure
typedef struct {
    uint16_t opcode; // Operation code (RRQ/WRQ/DATA/ACK/ERROR)
    union {
        struct {
            uint16_t  mode;  // reserved / unused
            char filename[256];
        } request;  // RRQ and WRQ

        struct {
            uint16_t block_number; // packet number
            char     data[512];
            int      data_size;
        } data_packet; // DATA

        struct {
            uint16_t block_number;
            int      data_size;
        } ack_packet; // ACK

        struct {
            uint16_t error_code;
            char     error_msg[512];
        } error_packet; // ERROR
    } body;
} tftp_packet;

#pragma pack(pop)

// Prototypes
void send_file(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, tftp_packet *packet);
void receive_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet);
int packet_fill_netascii(char *packet_data, int *packet_len, char *file_buffer, int file_bytes);
int packet_read_netascii(char *file_buffer, char *packet_data, int packet_bytes);

#endif // TFTP_H