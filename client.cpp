#include "packet.h"

int sockfd = -1;
int16_t ack_num = MAX_SEQ_NUM + 1;
int16_t seq_num = MAX_SEQ_NUM + 1;

void acknowlege(struct sockaddr_in& serv_addr, int type, int ack, bool retransmission) {
    struct packet ack_response = {
        timestamp(),
        type_ACK,
        1,  
        seq_num,   
        ack   
    };
    switch(type) {
        case type_SYN: ack_response.type += type_SYN; break;
        case type_FIN: ack_response.type += type_FIN; break;
        case type_ERR: ack_response.type += type_ERR; break;
    }
    send_packet(sockfd, serv_addr, ack_response, retransmission, false);
}

void request(char* filename, struct sockaddr_in& serv_addr, int16_t initial_seq_num) {
    seq_num = initial_seq_num;
    struct packet req_connection = {
        timestamp(),
        type_SYN,
        (int16_t) strlen(filename),
        seq_num,
        -1
    };
    strncpy((char*)req_connection.data, filename, strlen(filename));
    send_packet(sockfd, serv_addr, req_connection, false, false);
    seq_num += req_connection.len;
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
    
    int f = open("received.data", O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
    struct packet response;
    bool syn_acked = false;
    //struct timeval tv = {0, TIMEOUT*1000};
    struct timeval tv = {(10*TIMEOUT)/1000, 0};
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        fprintf(stderr, "ERROR: setsockopt() failed.\n");

    while(recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &src_addr, &addr_len) > 0) {
        //printf("Receiving packet %d\n\n", response.ack);
        switch(response.type) {
            case type_SYN + type_ACK: {
                if (!syn_acked && response.ack == initial_seq_num + strlen(filename) && response.ack == seq_num) {
                    ack_num = (response.seq + response.len) % MAX_SEQ_NUM;
                    acknowlege(serv_addr, type_SYN, ack_num, false);
                    syn_acked = true;
                }
                else 
                    acknowlege(serv_addr, type_SYN, ack_num, true);
            } break;
            case type_ERR: {
                if (response.seq == ack_num) {
                    seq_num++;
                    ack_num += response.len;
                    acknowlege(serv_addr, type_ERR, ack_num, false);
                    printf("---- Message from server: %s. ----\n", response.data);
                }
                else
                    acknowlege(serv_addr, type_ERR, ack_num, true);
            } break;
            case type_DATA: {
                if (response.seq == ack_num) {
                    write(f, response.data, response.len);
                    seq_num++;
                    ack_num += response.len;
                    if(ack_num > MAX_SEQ_NUM){
                        ack_num = ack_num % MAX_SEQ_NUM;
                    }
                    acknowlege(serv_addr, type_DATA, ack_num, false);
                }
                else {
                    printf("---- Unexpected data packet seq = %d. Wait for packet %d. ----\n", response.seq, ack_num);
                    acknowlege(serv_addr, type_DATA, ack_num, true);
                }
            } break;
            case type_FIN: {
                if (response.seq == ack_num) {
                    ack_num += response.len;
                    seq_num++;
                    acknowlege(serv_addr, type_FIN, ack_num, false);
                    struct timeval tv = {(10*TIMEOUT)/1000, 0}; // 3s
                    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                        fprintf(stderr, "ERROR: setsockopt() failed.\n");
                    }
                }
                else {
                    acknowlege(serv_addr, type_FIN, ack_num, true);
                }
            } break;
        }
        printf("Receiving packet %d\n\n", ack_num);    
    }
    if (ack_num == MAX_SEQ_NUM + 1) {
        printf("----- Fail to request, start request again. -----\n");
        goto retry;
    }
    close(f);
    close(sockfd);
}
