#include "packet.h"

int sockfd = -1;
int16_t ack_num = MAX_SEQ_NUM + 1;
int16_t seq_num = MAX_SEQ_NUM + 1;

void acknowlege(struct sockaddr_in& serv_addr, bool fin) {
    struct packet ack_response = {
        timestamp(),
        fin? (type_ACK + type_FIN) : type_ACK,
        1,  
        seq_num,   
        ack_num   
    };
    send_packet(sockfd, serv_addr, ack_response, false);
}

void request(char* filename, struct sockaddr_in& serv_addr, int16_t initial_seq_num) {
    struct packet req_connection = {
        timestamp(),
        type_SYN,
        1,
        initial_seq_num,
        -1
    };
    printf("this is client SYN to server as step 1 in line 26.\n");
    printf("- Sending SYN to server: seq = %d.\n", initial_seq_num);
    send_packet(sockfd, serv_addr, req_connection, false);
    
    seq_num = initial_seq_num + req_connection.len;
    struct packet response;
    if(recvfrom(sockfd, &response, sizeof(response) , 0, (struct sockaddr *) &serv_addr, &addr_len) < 0) {
        printf("- Waiting for ACK from the server.\n");
    }
    if(response.type != (type_SYN + type_ACK) || response.ack != seq_num) {
        printf("- Wrong SYN&ACK from the server.\n");
        return;
    }
    else{
        printf("- Received SYN&ACK from server. Now ACK back.\n");
    }

    ack_num = response.seq + response.len;
    struct packet req_file = {
        timestamp(),
        type_REQ + type_ACK,
        (int16_t) strlen(filename), 
        (int16_t) seq_num,
        (int16_t) ack_num
    };
    strncpy((char*)req_file.data, filename, strlen(filename));
    printf("this is client SYN&REQ to server as step 3 in line 52.\n");
    printf("- Sending request for \"%s\".\n", filename);
    send_packet(sockfd, serv_addr, req_file, false);
    seq_num += req_file.len;
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
    while(recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &src_addr, &addr_len) > 0) {
        printf("- Received packet %d.\n", response.seq);
        switch(response.type) {
            case type_DATA: {
                if (response.seq == ack_num) {
                    write(f, response.data, response.len);
                    ack_num += response.len;
                    if(ack_num > MAX_SEQ_NUM){
                        ack_num = 0;
                    }
                    
                    acknowlege(serv_addr, false);
                }
                else {
                    printf("- Fail to request file.\n");
                    goto retry;
                }
            } break;
            case type_FIN: {
                printf("- Received FIN. Sending FIN_ACK.\n");
                ack_num += response.len;
                acknowlege(serv_addr, true);
                struct timeval tv = {(2*TIMEOUT)/1000, 0}; // 1s
                if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                    fprintf(stderr, "ERROR: setsockopt() failed.\n");
                }
            } break;
        }
        
    }
    //if (ack_num == MAX_SEQ_NUM + 1) {
    //    printf("- Fail to request, start request again.\n");
    //    goto retry;
    //}
    close(f);
    //close(sockfd);
}
