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
#define CWND 5120

#define type_DATA 1
#define type_SYN 2
#define type_ACK 4
#define type_REQ 8
#define type_FIN 16
#define type_ERR 32

int duplicate_num = 0;
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
    if(is_server){
        printf("Sending packet %d 5120", packet.seq);
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
	sendto(sockfd, &packet, sizeof(packet) , 0, (struct sockaddr*) &addr, addr_len);
}
