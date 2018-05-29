#include "packet.h"

int sockfd = -1;
int16_t ack_num = MAX_SEQ_NUM + 1;
int16_t seq_num = MAX_SEQ_NUM + 1;

void acknowlege(struct sockaddr_in& serv_addr) {
    struct packet ack_response = {
        timestamp(),
        type_ACK,
        0,  
        seq_num,   
        ack_num   
    };
    send_packet(sockfd, serv_addr, ack_response, false);
}

void request(char* filename, struct sockaddr_in& serv_addr, int16_t initial_seq_num) {
    struct packet req_connection = {
        timestamp(),
        type_SYN,
        0,
        initial_seq_num,
        -1
    };
    printf("- Sending SYN to server: seq = %d.\n", initial_seq_num);
    send_packet(sockfd, serv_addr, req_connection, false);
    
    struct packet response;
    while(recvfrom(sockfd, &response, sizeof(response) , 0, (struct sockaddr *) &serv_addr, &addr_len) < 0) {
        printf("- Waiting for ACK from the server.\n");
    }
    if(response.type != (type_SYN + type_ACK) || response.ack != initial_seq_num + 1) {
        //send_packet(sockfd, serv_addr, req_connection, false);
        printf("- Wrong SYN&ACK from the server.\n");
        return;
    }

    seq_num = initial_seq_num + 1;
    ack_num = response.seq + 1;
    struct packet req_file = {
        timestamp(),
        type_REQ + type_ACK,
        (int16_t) strlen(filename), 
        (int16_t) seq_num,
        (int16_t) ack_num
    };
    strncpy((char*)req_file.data, filename, strlen(filename));
    printf("- Sending request for \"%s\": seq = %d, ack = %d.\n", filename, seq_num, response.seq+1);
    send_packet(sockfd, serv_addr, req_file, false);
}

int main(int argc, char *argv[])
{
    int portno;
    struct sockaddr_in serv_addr;
    struct sockaddr_in src_addr;
    socklen_t serv_len;
    socklen_t addr_len;
    struct hostent *server;
    char* hostname;

    if (argc < 4) {
        fprintf(stderr, "ERROR, no port or hostname or request provided\n");
        exit(1);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);
    char* filename = argv[3];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create socket
    if (sockfd < 0) {
        fprintf(stderr, "ERROR opening server socket\n");
        exit(1);
    }

    server = gethostbyname(hostname);

    if (!server) {
        fprintf(stderr, "ERROR obtaining host address of %s\n", hostname);
        exit(1);
    }

    memset((char*) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy((char*) &serv_addr.sin_addr, server->h_addr, server->h_length);

    int16_t initial_seq_num = randint(0, MAX_SEQ_NUM/2); //x
    retry: request(filename, serv_addr, initial_seq_num);
    
    struct packet response;
    while(recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &src_addr, &addr_len) > 0) {
        printf("- Received packet %d.\n", response.seq);
        switch(response.type) {
            case type_DATA: {
                if (ack_num == response.seq) {
                    printf("DATA: %s\n", (char*) response.data);
                    ack_num += response.len + 1;
                    acknowlege(serv_addr);
                }
                else {
                    printf("- Fail to request file.\n");
                    goto retry;
                }
            }
        }
        
    }
    if (ack_num == MAX_SEQ_NUM + 1) {
        printf("- Fail to request, start request again.\n");
        goto retry;
    }
    close(sockfd);
}
