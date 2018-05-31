#include "packet.h"

int sockfd = -1;

void process_request(struct sockaddr_in& cli_addr, const struct packet& request) {
	int16_t seq_num = randint(0, MAX_SEQ_NUM/2); //y
    int16_t ack_num = request.seq + request.len;
	struct packet ack_packet = {
	   	timestamp(),
	    type_ACK + type_SYN,
	    1,
	    (int16_t) seq_num,
	    (int16_t) ack_num
    };
    printf("- Sending ACK&SYN.\n");
    send_packet(sockfd, cli_addr, ack_packet, false);

    struct packet file_request;
    while(recvfrom(sockfd, &file_request, sizeof(file_request), 0, (struct sockaddr *) &cli_addr, &addr_len) < 0) {
    	printf("- Waiting for ACK&REQ.\n");
    }
    if(file_request.type != (type_ACK + type_REQ) || file_request.ack != seq_num + 1) {
    	printf("- ACK&REQ from client is wrong.\n");
    	return;
    }

    seq_num += ack_packet.len;
    ack_num += file_request.len;
    int f = open((char*) &file_request.data, O_RDONLY);
	if (f < 0) {
	    fprintf(stderr, "FAIL to open file\n");
	    exit(1);
	}
	struct packet data_packet = {
	    timestamp(),
	    type_DATA,
	    0,
	    (int16_t) seq_num,
	    (int16_t) ack_num
    };
    while((data_packet.len = read(f, data_packet.data, PAYLOAD_SIZE)) > 0) {
        printf("- Sending DATA.\n");
		send_packet(sockfd, cli_addr, data_packet, false);
        seq_num += data_packet.len;
    }

    seq_num += data_packet.len;
    struct packet file_ack;
    while(recvfrom(sockfd, &file_ack, sizeof(file_ack), 0, (struct sockaddr *) &cli_addr, &addr_len) < 0) {
        printf("- Waiting for ACK.\n");
    }
    if(file_ack.type != type_ACK or file_ack.ack != seq_num) {
        printf("- ACK is wrong.\n");
        return;
    }

    struct packet fin_packet = {
        timestamp(),
        type_FIN,
        1,
        (int16_t) seq_num,
        -2 //???
    };
    printf("- Sending FIN.\n");
    send_packet(sockfd, cli_addr, fin_packet, false);

    seq_num += fin_packet.len;
    struct packet fin_ack;
    while(recvfrom(sockfd, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr *) &cli_addr, &addr_len) < 0) {
        printf("- Waiting for FIN&ACK.\n");
    }
    if(fin_ack.type != (type_ACK + type_FIN) or fin_ack.ack != seq_num) {
        printf("- FIN&ACK is wrong.\n");
        return;
    }
}

int main(int argc, char *argv[])
{
    int portno;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create UDPsocket
    if (sockfd < 0) {
        fprintf(stderr, "ERROR opening server socket\n");
        exit(1);
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));   // reset memory
    // fill in address info
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "ERROR on binding\n");
        exit(1);
    }

    //process one file request per loop
    while(1){
    	struct packet request;
        int recv_len = recvfrom(sockfd, &request, sizeof(request), 0, (struct sockaddr *) &cli_addr, &addr_len);
        if (recv_len < 0) {
            fprintf(stderr,"FAIL to read from socket. ERROR: %s\n", strerror(errno));
            exit(1);
        }
        if(request.type == type_SYN) {
        	printf("- Received SYN. Start processing new request.\n");
        	process_request(cli_addr, request);
        }
        else 
        	printf("- Warning: Unrecognized packet.\n");
    }
    close(sockfd);
}