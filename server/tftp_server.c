#include "tftp.h"
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT 6969

void handle_client(int sockfd, char *buffer, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet* packet);

int main()
{
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    tftp_packet packet;
    memset(&packet, 0, sizeof(packet));
    
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        printf("Socket creation failed\n");
        return 1;
    }

    // Bind the socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int ret = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(ret < 0){
        perror("bind");
        printf("Bind failed\n");
        close(sockfd);
        return 1;
    }

    printf("TFTP Server listening on port %d...\n", PORT);

    // Main loop to handle incoming requests
    while (1) {
        int n = recvfrom(sockfd, &packet, sizeof(packet), 0,
                         (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            perror("recvfrom (request)");
            printf("Data not received\n");
            continue;
        }

        handle_client(sockfd, (char *)&packet, client_addr, client_len, &packet);
    }

    close(sockfd);
    return 0;
}

void handle_client(int sockfd, char *buffer, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet)
{
    //tftp_packet *packet = (tftp_packet *)buffer;
    int mode = (packet->body.request.mode == 2) ? 1 : 512;
    // Determine operation (PUT / GET)
    if (packet->opcode == WRQ) {
        // Client wants to upload (PUT): create file and ACK(0)
        int fd = open(packet->body.request.filename,
                      O_CREAT | O_WRONLY | O_TRUNC,
                      0666);
        if (fd < 0) {
            perror("open (WRQ)");
            tftp_packet err;
            err.opcode                            = ERROR;
            err.body.error_packet.error_code      = 2; // Access violation / file error
            strcpy(err.body.error_packet.error_msg, "File creation failed");
            sendto(sockfd, &err, sizeof(err), 0,
                   (struct sockaddr *)&client_addr, client_len);
            printf("File creation failed for WRQ\n");
            return;
        }
        close(fd);

        // Send ACK(0) to start data transfer
        tftp_packet ack;
        ack.opcode                       = ACK;
        ack.body.ack_packet.block_number = 0;

        if (sendto(sockfd, &ack, sizeof(ack), 0,
                   (struct sockaddr *)&client_addr, client_len) < 0) {
            perror("sendto (WRQ ACK)");
            return;
        }

        printf("WRQ received for '%s', ACK(0) sent\n", packet->body.request.filename);

        // Now receive the file
        receive_file(sockfd, client_addr, client_len, packet);
    }
    else if (packet->opcode == RRQ) {
        int fd = open(packet->body.request.filename, O_RDONLY);
        if(fd < 0){
            perror("open (RRQ)");
            tftp_packet err;
            err.opcode                            = ERROR;
            err.body.error_packet.error_code      = 1; // Access violation / file error
            strcpy(err.body.error_packet.error_msg, "File not found");
            sendto(sockfd, &err, sizeof(err), 0,
                   (struct sockaddr *)&client_addr, client_len);
            return;
        }
        printf("RRQ received for '%s'\n", packet -> body.request.filename);
        
        send_file(sockfd, &client_addr, client_len, packet);
    }
    else {
        printf("Unknown opcode: %d\n", packet->opcode);
    }
}