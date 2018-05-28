#include "packet.h"

int sockfd = -1;

void request(char* filename, struct sockaddr_in& serv_addr, int16_t seq_num) {
    struct packet req_connection = {
        timestamp(),
        type_SYN,
        0,
        seq_num,
        -1
    };
    printf("- Sending request for SYN.\n");
    send_packet(sockfd, serv_addr, req_connection, false);
    struct packet response;
    socklen_t addr_len;
    int recv_len = recvfrom(sockfd, &response, sizeof(response) , 0, (struct sockaddr *) &serv_addr, &addr_len);
    int next_seq = seq_num + 1;
    while(response.type != 6 || response.ack != next_seq) {
        send_packet(sockfd, serv_addr, req_connection, false);
    }
    struct packet req_file = {
        timestamp(),
        type_REQ & type_ACK,
        (int16_t) strlen(filename), 
        (int16_t) next_seq,
        (int16_t) (response.seq + 1)
    };
    strncpy((char*)req_file.data, filename, strlen(filename));
    printf("- Sending request for \"%s\".\n", filename);
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

    int16_t seq_num = randint(0, MAX_SEQ_NUM/2); //x
    request(filename, serv_addr, seq_num);

    while(1) {
        struct packet response;
    	int recv_len = recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &src_addr, &addr_len);
        if (recv_len < 0) {
            fprintf(stderr,"FAIL to read from socket. ERROR: %s\n", strerror(errno));
            exit(1);
        }
        printf("%s\n", (char*) response.data);
    }

    close(sockfd);
}
