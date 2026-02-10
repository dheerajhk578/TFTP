/* Server-side file transfer helpers */

#include "tftp.h"
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>


// Send file to client (handles RRQ)
void send_file(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, tftp_packet *packet) {
    uint16_t ser_block = 1;
    int bytes_read = 0;
    tftp_packet data_pkt, ack_pkt;
    int read_next = 1; // Flag: Only read if the last block was ACKed
    int g_mode = packet->body.request.mode;
     //printf("mode = %d\n", current_mode);

    int fd = open(packet->body.request.filename, O_RDONLY);
    if (fd < 0) {
        perror("File open failed");
        return;
    }
    
    while (1) {
        if (read_next) {
            memset(&data_pkt, 0, sizeof(data_pkt));
            int size_to_read = (g_mode == 2) ? 1 : 512;

            if (g_mode == 0) { // Netascii Mode
                // Create a temp buffer to hold raw file data
                char temp_buf[512]; 
            
                // Read from file into temp_buf
                int raw_read = read(fd, temp_buf, size_to_read);
            
                if (raw_read < 0) {
                    break; 
                }

                // CONVERT: Use helper function
                int pkt_len = 0;
                int consumed = packet_fill_netascii(data_pkt.body.data_packet.data, &pkt_len, temp_buf, raw_read);
            
                // If we didn't use all the bytes we read (because packet got full),
                // we must rewind the file pointer so we read them again next time.
                if (consumed < raw_read) {
                    lseek(fd, -(raw_read - consumed), SEEK_CUR);
                }
                data_pkt.opcode                         = DATA;
                data_pkt.body.data_packet.block_number  = ser_block;
                data_pkt.body.data_packet.data_size     = pkt_len;
                bytes_read = pkt_len;
            }
            else{
                bytes_read = read(fd, data_pkt.body.data_packet.data, size_to_read);
            
                if (bytes_read < 0) {
                    perror("Read error");
                    break;
                }
                data_pkt.opcode = DATA;
                data_pkt.body.data_packet.block_number = ser_block;
                data_pkt.body.data_packet.data_size = bytes_read;
            }
                read_next = 0; 
        }

        // We MUST send enough bytes so the receiver can "see" the data_size field 
        // which sits at the end of the 512-byte buffer.
        size_t packet_len = sizeof(uint16_t) + sizeof(uint16_t) + 512 + sizeof(int);
    
        sendto(sockfd, &data_pkt, packet_len, 0, (struct sockaddr *)client_addr, client_len);

        // Wait for ACK
        printf("Sent block %d of %d bytes\n", ser_block, bytes_read);
        socklen_t addr_len = client_len;
        ssize_t n = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)client_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom failed (ACK)");
            printf("ACK not received, resending the same block %d\n", ser_block);
            continue; // resend same block
        }
        if (ack_pkt.opcode == ACK && ack_pkt.body.ack_packet.block_number == ser_block) {
            printf("ACK received for block %d (%d bytes)\n", ser_block, bytes_read);
            if (bytes_read == 0) {
                break; // Last packet was ACKed
            }
            ser_block++;
            read_next = 1; 
        } 
        else {
            printf("ACK not received/mismatched, resending block %d\n", ser_block);
            continue; // resend same block
        }
    }
        close(fd);
        printf("Transfer finished.\n");
}

// Receive file from client (handles WRQ)
void receive_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet)
{
    int g_mode = packet->body.request.mode; // Detect mode from request
    int fd = open(packet->body.request.filename,
                  O_CREAT | O_WRONLY | O_TRUNC,
                  0666);
    if (fd < 0) {
        perror("open (receive_file)");
        printf("File opening/creation failed\n");
        return;
    }

    uint16_t ser_block = 1;
    tftp_packet data_pkt, ack_pkt;

    while (1) {
        // Receive data packet
        socklen_t addr_len = client_len;
        ssize_t n = recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                             (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom (DATA)");
            printf("Data receive failed\n");
            continue;
        }
        uint16_t received_opcode = data_pkt.opcode;
        uint16_t received_block  = data_pkt.body.data_packet.block_number;
        int bytes                = data_pkt.body.data_packet.data_size;
        // Check block number
        if (received_block == ser_block && received_opcode == DATA) {
            // Write data to file
            ssize_t written = write(fd, data_pkt.body.data_packet.data, bytes);
            if (written < 0) {
                perror("write");
                break;
            }

            printf("Received block %d of %d bytes\n", ser_block, bytes);

            // Send ACK for this block
            memset(&ack_pkt, 0, sizeof(ack_pkt));
            ack_pkt.opcode                        = ACK;
            ack_pkt.body.ack_packet.block_number  = ser_block;

            size_t ack_len = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(int);
            ssize_t sent = sendto(sockfd, &ack_pkt, ack_len, 0,
                                  (struct sockaddr *)&client_addr, client_len);
            if (sent < 0) {
                perror("sendto (ACK)");
                break;
            }

            // Stopping condition: only after correct block processed
            if (bytes == 0) {
                printf("File received without error\n");
                break;
            }
            ser_block++;
        }
        else if (received_opcode == DATA && received_block < ser_block){
            // Re-ACK the duplicate block using the incoming block number
            memset(&ack_pkt, 0, sizeof(ack_pkt));
            ack_pkt.opcode                       = ACK;
            ack_pkt.body.ack_packet.block_number = received_block;
            
            size_t ack_len = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(int);
            sendto(sockfd, &ack_pkt, ack_len, 0, (struct sockaddr *)&client_addr, client_len);
            printf("Resent ACK for block %d\n", received_block);
        }
    }
     close(fd);
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
                // Not enough space! Stop here so this \n is handled in the NEXT block
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