#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H
 
typedef struct {
    int sockfd; // collect fd of the socket
    struct sockaddr_in server_addr; //information about server
    socklen_t server_len; // len of the server
    char server_ip[INET_ADDRSTRLEN]; //for ip address
    int port;                        // port number
} tftp_client_t;

// Function prototypes
void connect_to_server(tftp_client_t *client);
void put_file(tftp_client_t *client);
void get_file(tftp_client_t *client);
void disconnect(tftp_client_t *client); 
//void process_command(tftp_client_t *client, char *command); not required


void send_request(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, tftp_packet *packet, tftp_opcode opcode); //put opcode - to check the operation {get or put}
void receive_request(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len, tftp_packet *packet, tftp_opcode opcode); //get

#endif