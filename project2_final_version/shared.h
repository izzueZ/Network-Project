
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <list>
#include <set>
using std::list;
using std::set;

socklen_t addrlen = sizeof(struct sockaddr_in);
bool diag = false;  

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))


#define SR_SYN 1  
#define SR_STATUS 2  
#define SR_DATA 3  
#define SR_ACK 4
#define SR_FIN 5

#define SR_MAX_SEQNUM 30720
#define SR_TIMEOUT 500   

#define SR_SS 0
#define SR_CA 1
#define SR_FR 2


#define SR_PKT_SIZE 1024
#define SR_MAX_LEN (SR_PKT_SIZE - 16)

void error(const char msg[]) {
	perror(msg);
	exit(1);
}


int64_t timestamp() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

int randint(int min, int max) {
	srand(timestamp());
	return (double)rand()/(double)RAND_MAX * (max - min) + min;
}

struct sr_packet {
	int64_t timestamp;
	int16_t type;	
	int16_t len;
	int16_t seq;	
	int16_t ack;	
	int8_t data[SR_MAX_LEN];
};

int mode = 0, cwnd = 1024, ssthresh = 15360, dupackcount = 0;

void set_cwnd(int val) {
	cwnd = min(val, SR_MAX_SEQNUM/2);
}


struct sockaddr_in generate_serv_addr(int portno, const char hostname[]) {
	struct sockaddr_in serv_addr;
	memset((char*)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	if (hostname == NULL)
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	else {
		struct hostent* server = gethostbyname(hostname);
		if (server == NULL) {
			fprintf(stderr, "Error: gethostbyname() failed: no such host as "
				"%s\n", hostname);
			exit(1);
		}
		memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, 
			server->h_length);
	}
	return serv_addr;
}


void send_packet(int sockfd, struct sockaddr_in& addr,
	const struct sr_packet& packet, bool retransmission) {
	printf("Sending packet %d", 
		packet.type == SR_ACK ? packet.ack : packet.seq);
	if (packet.ack == -2)  // from server
		printf(" %d", cwnd);
	if (retransmission)
		printf(" Retransmission");
	switch (packet.type) {
		case SR_SYN:
			printf(" SYN");
			break;
		case SR_STATUS:
			if (diag)
				printf(" STATUS \"%s\"", packet.data);
			break;
		case SR_DATA:
			if (diag)
				printf(" DATA");
			break;
		case SR_ACK:
			if (packet.seq == -2)  // FIN ACK
				printf(" FIN");
			if (diag)
				printf(" ACK");
			break;
		case SR_FIN:
			printf(" FIN");
			break;
	}
	printf("\n");
	if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)&addr, 
		addrlen) < 0)
		error("Error: sendto() failed");
}