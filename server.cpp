#include "packet.h"

int sockfd = -1;

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
    	int16_t seq_num = randint(0, MAX_SEQ_NUM/2); //y
        int recv_len = recvfrom(sockfd, &request, sizeof(request), 0, (struct sockaddr *) &cli_addr, &addr_len);
        if (recv_len < 0) {
            fprintf(stderr,"FAIL to read from socket. ERROR: %s\n", strerror(errno));
            exit(1);
        }
        if(request.type == type_SYN) {
        	printf("- Received SYN.\n");
        	struct packet ack_packet = {
	           	timestamp(),
	            type_ACK & type_SYN,
	           	0,
	           	(int16_t) seq_num,
	           	(int16_t) (request.seq + 1)
        	};
        	printf("- Sending ack for SYN.\n");
        	send_packet(sockfd, cli_addr, ack_packet, false);
        }
        else 
        	continue;

        int next_seq = seq_num + 1;
        recv_len = recvfrom(sockfd, &request, sizeof(request), 0, (struct sockaddr *) &cli_addr, &addr_len);
        if (recv_len < 0) {
            fprintf(stderr,"FAIL to read from socket. ERROR: %s\n", strerror(errno));
            exit(1);
        }
        if(request.type == 12 && request.ack == next_seq) {
        	int f = open((char*) &request.data, O_RDONLY);
	        if (f < 0) {
	        	fprintf(stderr, "FAIL to open file\n");
	        	exit(1);
	        }
	        struct packet data_packet = {
	           	timestamp(),
	            type_DATA,
	           	0,
	           	(int16_t) seq_num,
	           	-2
        	};
        	while((data_packet.len = read(f, data_packet.data, PAYLOAD_SIZE)) > 0) {
        		printf("- Sending data for REQ.\n");
				send_packet(sockfd, cli_addr, data_packet, false);
        	}
        }
    }

    close(sockfd);

}
