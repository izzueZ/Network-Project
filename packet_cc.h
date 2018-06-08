#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <errno.h>
#include <netdb.h>
#include <cstring>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/time.h>
#include <algorithm>

#include <iostream>
#include <fstream>
#include <sstream>
#include <list>

using namespace std;

#define BUF_SIZE 1024 //bytes
#define MAX_PACKET_SIZE 1024 //bytes
#define HEADER_SIZE 16 //bytes
#define MAX_SEQ_NUM 30720 //bytes
#define PAYLOAD_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)
#define TIMEOUT 500


#define type_DATA 1
#define type_SYN 2
#define type_ACK 4
#define type_REQ 8
#define type_FIN 16
#define type_ERR 32

#define SS 1
#define CA 2
#define FR 3

int duplicate_num = 0;
int ssthresh = 15360;
int CWND = 1024;
int mode = SS;
socklen_t addr_len = sizeof(struct sockaddr_in);

int64_t timestamp() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

int randint(int min, int max) {
	srand(timestamp());
	return (double)rand()/(double)RAND_MAX * (max - min) + min;
}

struct packet {
	int64_t timestamp;
	int16_t type;	
	int16_t len;
	int16_t seq;	
	int16_t ack;	
	int8_t data[PAYLOAD_SIZE];
};

void send_packet(int sockfd, struct sockaddr_in& addr, const struct packet& packet, bool retransmission, bool is_server) {
	//printf("Sending packet seq = %d, ack = %d, len = %d", packet.seq, packet.ack, packet.len);
    if(is_server){
        printf("Sending packet %d %d %d", packet.seq, CWND, ssthresh);
    }
    else{
        printf("Sending packet %d", packet.seq);
    }
    
	if(retransmission)
		printf(" Retransmission");
	if((packet.type & type_SYN) == type_SYN)
		printf(" SYN");
	if((packet.type & type_FIN) == type_FIN)
		printf(" FIN");
	printf("\n\n");

	/*if((packet.type & type_DATA) == type_DATA)
		printf("DATA ");
	if((packet.type & type_SYN) == type_SYN)
		printf("SYN ");
	if((packet.type & type_ACK) == type_ACK)
		printf("ACK ");
	if((packet.type & type_REQ) == type_REQ)
		printf("REQ ");
	if((packet.type & type_FIN) == type_FIN)
		printf("FIN ");
	printf("   seq = %d, ack = %d, len = %d.\n\n", packet.seq, packet.ack, packet.len);
	*/
	//cout << packet.len << endl;
	sendto(sockfd, &packet, sizeof(packet) , 0, (struct sockaddr*) &addr, addr_len);
}
