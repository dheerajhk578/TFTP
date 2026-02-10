#include "tftp.h"
#include "tftp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include<fcntl.h>

int g_mode = 1;
int main() {
    tftp_client_t client;
    memset(&client, 0, sizeof(client));  // Initialize client structure
    int choice;
    tftp_packet packet;

    // Main loop for command-line interface
    while (1) {
            //print the main menu
            printf("1.Connect\n");
            printf("2.Put\n");
            printf("3.Get\n");
            printf("4.Mode\n");
            printf("5.Exit\n");

            //read the choice from the user
            printf("Enter the choice: ");
            scanf("%d", &choice);
            //based on the choice perform the operation
             switch(choice){
                case 1:
                    connect_to_server(&client);
                    break;
                case 2:
                    put_file(&client);
                    break;
                case 3:
                    get_file(&client);
                    break;
                case 4:
                    printf("Select the mode:\n");
                    printf("1. Default(512 bytes)\n2. Octet(1 byte)\n3. NETASCII\nEnter your choice:");
                    scanf("%d", &g_mode);
                    break;
                case 5:
                    printf("Thank you, exiting🙏\n");
                    exit(0);
                default:
                    printf("Invalid choice\n");
             }
            
    }

    return 0;
}

// This function is to initialize socket with given server IP, no packets sent to server in this function
void connect_to_server(tftp_client_t *client)//passing structure
{
    int i = 0, dot = 0;
    // Create UDP socket
    client -> sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   
    if(client -> sockfd <= 0)
    {
        perror("Socket creating failed");
        return;
    } 

    // read the server addresss and port number
    printf("Enter the IP address: ");
    scanf("%s", client -> server_ip);
    printf("Enter the port number: ");
    scanf("%d", &client -> port);

    //validate the information
    while(client -> server_ip[i]){
        if((client -> server_ip[i] >= '0' && client -> server_ip[i] <= '9') || client -> server_ip[i] == '.'){
            if(client -> server_ip[i] == '.'){
                dot++;
                if(!(client -> server_ip[i + 1] >= '0' && client -> server_ip[i + 1] <= '9')){
                    printf("Invalid IP address\n");
                    return;
                }
            }
        }
        else{
            printf("Invalid IP address\n");
            return;
        }
        i++;
    }
    if(dot != 3){
        printf("Invalid IP address\n");
        return;
    }

    if(client -> port <= 1024 || client -> port >= 65535){
        printf("Invalid port number\n");
        return;
    }

    //bind the network parameters
    client -> server_addr.sin_family = AF_INET;
    client -> server_addr.sin_port = htons(client -> port);
    client -> server_addr.sin_addr.s_addr = inet_addr(client->server_ip);
    client -> server_len = sizeof(client -> server_addr);
    //int ret = bind(client -> sockfd, (struct sockaddr *)&client -> server_addr, sizeof(client -> server_addr));

    printf("Connection sent to the server\n");
}

void put_file(tftp_client_t *client) {
    if (client->sockfd <= 0) {
        printf("Not connected to server. Use Connect first.\n");
        return;
    }

    tftp_packet packet;
    memset(&packet, 0, sizeof(packet));

    // Read the filename from the user
    printf("Enter the filename to upload (PUT): ");
    scanf("%255s", packet.body.request.filename);
    packet.body.request.mode = g_mode;

    // Validate that the file exists locally
    int fd = open(packet.body.request.filename, O_RDONLY);
    if (fd < 0) {
        printf("File not found\n");
        return;
    }
    close(fd);

    // Send WRQ and then the file
    send_request(client->sockfd, &client->server_addr, client->server_len,
                 &packet, WRQ);
}

void send_request(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len,
                  tftp_packet *packet, tftp_opcode opcode)
{
    // Prepare WRQ (opcode + filename + trailing '\0')
    packet->opcode = opcode;

    size_t filename_len = strlen(packet->body.request.filename);
    // opcode (2 bytes) + filename + '\0'
    size_t size = sizeof(packet->opcode) + filename_len + 1;

    ssize_t sent = sendto(sockfd, packet, sizeof(tftp_packet), 0,
                          (struct sockaddr *)server_addr, server_len);
    if (sent < 0) {
        perror("sendto failed (WRQ)");
        return;
    }

    // Wait for server ACK (block 0) or ERROR
    tftp_packet ack_packet;
    socklen_t addr_len = server_len;
    ssize_t ack = recvfrom(sockfd, &ack_packet, sizeof(ack_packet), 0,
                           (struct sockaddr *)server_addr, &addr_len);
    if (ack < 0) {
        perror("recvfrom failed (WRQ ACK)");
        printf("ACK failed\n");
        return;
    }

    if (ack_packet.opcode == ACK && ack_packet.body.ack_packet.block_number == 0) {
        printf("Server ACK for WRQ received successfully\n");

        // Now send the file data
        send_file(sockfd, server_addr, addr_len, packet);
    } 
    else if (ack_packet.opcode == ERROR) {
        printf("Server sent ERROR for WRQ\n");
        return;
    }
    else {
        printf("Unexpected response to WRQ, server rejected file\n");
        return;
    }
}

void get_file(tftp_client_t *client){
    if (client->sockfd <= 0) {
        printf("Not connected to server. Use Connect first.\n");
        return;
    }

    tftp_packet packet;
    memset(&packet, 0, sizeof(packet));

    // Read the filename from the user
    printf("Enter the filename to receive (GET): ");
    scanf("%255s", packet.body.request.filename);
    packet.body.request.mode = g_mode;

    // Send RRQ
    receive_request(client->sockfd, &client->server_addr, client->server_len,
                 &packet, RRQ);
    //Wait for DATA
    receive_file(client->sockfd, client->server_addr, client->server_len, &packet);
}

void receive_request(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, tftp_packet *packet, tftp_opcode opcode){
    //setting opcode
    packet->opcode = opcode;
    tftp_packet err;
    // opcode (2 bytes) + filename + '\0'
    //size_t filename_len = strlen(packet -> body.request.filename);
    //uint16_t mode = strlen(packet->body.request.mode);
    //size_t size = sizeof(packet->opcode) + filename_len + 1;

    ssize_t sent = sendto(sockfd, packet, sizeof(tftp_packet), 0, (struct sockaddr *)server_addr, server_len);

    if(sent < 0){
        perror("Sendto failed(RRQ)");
        return;
    }
    int n = recvfrom(sockfd, &err, sizeof(err), 0, (struct sockaddr*)server_addr, &server_len);
    if(err.opcode == ERROR){
        printf("Opcode ERROR received, file not found\n");
        return;
    }
    printf("Request sent: opcode = %d filename = %s\n", 
        opcode, packet -> body.request.filename);
}
