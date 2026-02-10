/* Common file for server & client */

#include "tftp.h"
#include "tftp_client.h"
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include<fcntl.h>

//for put (from client to server)
void send_file(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, tftp_packet *packet) {
    //printf("DEBUG : Mode received is %d\n", g_mode);
    uint16_t cli_block  = 1;
    int bytes_read      = 0;
    int read_next       = 1; // Flag: Only read if the last block was ACKed
    int g_mode = packet->body.request.mode; 
    tftp_packet data_pkt, ack_pkt;
    
    int fd = open(packet->body.request.filename, O_RDONLY);
    if (fd < 0) {
        printf("File open failed\n");
        return;
    }

    while (1) {
        if(read_next){
            memset(&data_pkt, 0, sizeof(data_pkt));
            int size_to_read = (g_mode == 2) ? 1 : 512;
            if (g_mode == 0) { // Netascii Mode
                char temp_buf[512]; 
                int raw_read = read(fd, temp_buf, size_to_read); // size_to_read acts as limit
                
                if (raw_read < 0) { perror("Read error"); break; }

                int pkt_len = 0;
                int consumed = packet_fill_netascii(data_pkt.body.data_packet.data, &pkt_len, temp_buf, raw_read);
                
                // Rewind file if Netascii expansion filled the packet early
                if (consumed < raw_read) {
                    lseek(fd, -(raw_read - consumed), SEEK_CUR);
                }
            
                // Prepare DATA packet
                data_pkt.opcode                         = DATA;
                data_pkt.body.data_packet.block_number  = cli_block;
                data_pkt.body.data_packet.data_size     = pkt_len;
                bytes_read = pkt_len;
            }
            else { // Octet Mode
                bytes_read = read(fd, data_pkt.body.data_packet.data, size_to_read);
                if (bytes_read < 0) { perror("Read error"); break; }

                data_pkt.opcode                        = DATA;
                data_pkt.body.data_packet.block_number = cli_block;
                data_pkt.body.data_packet.data_size    = bytes_read;
            }
            read_next = 0; 
        }

        // Send header (2 bytes opcode + 2 bytes block) + data
        ssize_t sent = sendto(sockfd, &data_pkt, sizeof(data_pkt), 0,
                              (struct sockaddr *)server_addr, server_len);
        if (sent < 0) {
            perror("sendto failed");
            break;
        }
        printf("Sent block %d of %d bytes\n", cli_block, bytes_read);

        // Wait for ACK for this block
        ssize_t n = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0,
                             (struct sockaddr *)server_addr, &server_len);
        if (n < 0) {
            perror("recvfrom failed (ACK)");
            printf("ACK not received, resending the same block %d\n", cli_block);
            continue; // resend same block
        }

        if (ack_pkt.opcode == ACK && ack_pkt.body.ack_packet.block_number == cli_block) {
            printf("ACK received for block %d (%d bytes)\n", cli_block, bytes_read);
            // Last block:
            if (bytes_read == 0) {
                printf("File transfer completed\n");
                break;
            }
            cli_block++;
            read_next = 1;
        } 
        else{
            printf("ACK not received/mismatched, resending block %d\n", cli_block);
            continue; // resend same block
        }

    }

    close(fd);
    printf("File sent successfully\n");
}

//for get (Receive from server to client)
void receive_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet) 
{
    int g_mode = packet->body.request.mode; // Detect mode from request
    int fd = open(packet->body.request.filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) {
        perror("File creation failed");
        return;
    }

    uint16_t ser_block = 1;
    tftp_packet data_pkt, ack_pkt;

    while (1) {
        socklen_t addr_len = client_len;
        ssize_t n = recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0, (struct sockaddr *)&client_addr, &addr_len);

        if (n < 0){
            perror("Recvfrom failed");
            continue;
        } 

        uint16_t opcode = data_pkt.opcode;
        uint16_t block  = data_pkt.body.data_packet.block_number;
        int data_size   = data_pkt.body.data_packet.data_size;

        if (opcode == DATA && block == ser_block) {
            write(fd, data_pkt.body.data_packet.data, data_size);
            printf("Received block %d (%d bytes)\n", block, data_size);

            // Send ACK
            memset(&ack_pkt, 0, sizeof(ack_pkt));
            ack_pkt.opcode = ACK;
            ack_pkt.body.ack_packet.block_number = block;

            // Send opcode + block_number + data_size
            size_t ack_len = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(int);
            sendto(sockfd, &ack_pkt, ack_len, 0, (struct sockaddr *)&client_addr, client_len);

            if (data_size == 0){
                break;
            } 
            ser_block++;
        } 
        else if (opcode == DATA && block < ser_block) {
            // Duplicate: Resend ACK for what we just got
            ack_pkt.opcode = ACK;
            ack_pkt.body.ack_packet.block_number = block;
            size_t ack_len = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(int);
            sendto(sockfd, &ack_pkt, ack_len, 0, (struct sockaddr *)&client_addr, client_len);
            printf("Duplicate block %d, resent ACK\n", block);
        }
    }
    close(fd);
    printf("File saved successfully.\n");
}

int packet_fill_netascii(char *packet_data, int *packet_len, char *file_buffer, int file_bytes) {
    int p_idx = 0; // packet index
    int f_idx = 0; // file index

    while (f_idx < file_bytes && p_idx < 512) {
        char c = file_buffer[f_idx];

        if (c == '\n') {
            // Need 2 bytes for \r\n. Check if we have space.
            if (p_idx + 1 < 512) {
                packet_data[p_idx++] = '\r';
                packet_data[p_idx++] = '\n';
                f_idx++; // We consumed this newline from the file
            } else {
                // Not enough space, so this \n is handled in the NEXT block
                break; 
            }
        } 
        else if (c == '\r') {
            // Need 2 bytes for \r\0. Check space.
            if (p_idx + 1 < 512) {
                packet_data[p_idx++] = '\r';
                packet_data[p_idx++] = '\0';
                f_idx++;
            } else {
                break;
            }
        } 
        else {
            packet_data[p_idx++] = c;
            f_idx++;
        }
    }
    
    *packet_len = p_idx; // how big the packet is
    return f_idx;        // how many file bytes we processed
}

// Returns: Number of bytes to write to the file
int packet_read_netascii(char *file_buffer, char *packet_data, int packet_bytes) {
    int f_idx = 0;
    for (int i = 0; i < packet_bytes; i++) {
        // Found a CR?
        if (packet_data[i] == '\r') {
            // Check if next char is LF (\n)
            if (i + 1 < packet_bytes && packet_data[i+1] == '\n') {
                file_buffer[f_idx++] = '\n'; // Turn \r\n into just \n
                i++; // Skip the \n in the packet
            }
            // Check if next char is NULL (\0)
            else if (i + 1 < packet_bytes && packet_data[i+1] == '\0') {
                file_buffer[f_idx++] = '\r'; // Turn \r\0 into just \r
                i++; // Skip the \0
            }
            else {
                file_buffer[f_idx++] = packet_data[i];
            }
        } else {
            file_buffer[f_idx++] = packet_data[i];
        }
    }
    return f_idx;
}