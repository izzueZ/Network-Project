
#include "shared.h" 


void ack_response(int sockfd, struct sockaddr_in& serv_addr, int16_t seq,
	bool retransmission, bool finack) {
	struct sr_packet ACK = {
		timestamp(),
		SR_ACK,
		1,  
		finack ? -2 : -1,   
		seq   
	};
	send_packet(sockfd, serv_addr, ACK, retransmission);
}

void request(int sockfd, struct sockaddr_in& serv_addr, 
	const char filename[], int16_t initial_seqnum) {
	printf("* Sending request for \"%s\".\n", filename);
	struct sr_packet request = {
		timestamp(),
		SR_SYN,
		(int16_t)strlen(filename),  
		initial_seqnum,  
		-1  
	};
	strncpy((char*)request.data, filename, strlen(filename));
	send_packet(sockfd, serv_addr, request, false);
}

 

int main(int argc, char* argv[]) { 
	if (argc == 5 && strcmp(argv[4], "diag") == 0)
		diag = true;
	else if (argc != 4) {
		fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
		exit(1);
	}
	const char* hostname = argv[1], *filename = argv[3];
	int portno = atoi(argv[2]);
	if (portno < 0) {
		fprintf(stderr, "%d is not a valid port number.\n", portno);
		exit(1);
	}
	 
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("Error: socket() failed");
	struct sockaddr_in serv_addr = generate_serv_addr(portno, hostname);
	int16_t initial_seqnum = randint(0, SR_MAX_SEQNUM/2);
	retry: request(sockfd, serv_addr, filename, initial_seqnum);
    printf("this is the request of SYN from client as strep 1.\n");
    
	int f = open("received.data", O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
	list<sr_packet> window;
	int16_t next_seqnum = initial_seqnum + 1;
	struct sr_packet response;
	struct timeval tv = {0, SR_TIMEOUT*1000};  // 500 ms, until SYN received
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		error("Error: setsockopt() failed");
	 
	int n;
	while ((n = recvfrom(sockfd, &response, sizeof(response), 0, 
		(struct sockaddr*)&serv_addr, &addrlen)) > 0) {
		printf("Receiving packet %d\n", response.seq);
		switch (response.type) {
			case SR_SYN: {
				if (next_seqnum == initial_seqnum + 1) {
                    printf("this is the ack from client response to the server as step 3.\n");
                    //so we need an ack here
					ack_response(sockfd, serv_addr, response.seq, false, false);
					next_seqnum++;
					struct timeval tv = {0, 0};
					if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, 
						sizeof(tv)) < 0)
						error("Error: setsockopt() failed");
				}
				else  // duplicate
					ack_response(sockfd, serv_addr, response.seq, true, false);
				
			} break;
			case SR_STATUS: {
				if (response.seq == next_seqnum) {
					ack_response(sockfd, serv_addr, response.seq, false, false);
					next_seqnum++;
					printf("* Message from server: %s\n", response.data);
				}
				else  // duplicate
					ack_response(sockfd, serv_addr, response.seq, true, false);
			} break;
			case SR_DATA: {
                //this is duplicate case
				if ((response.seq < next_seqnum 
					&& response.seq >= next_seqnum - SR_MAX_SEQNUM/2)
					|| (next_seqnum == 0 && response.seq >= SR_MAX_SEQNUM/2))
					ack_response(sockfd, serv_addr, response.seq, true, false); 
				else if (response.seq == next_seqnum) {
					ack_response(sockfd, serv_addr, response.seq, false, false);
					write(f, response.data, response.len);
					if ((next_seqnum += response.len) > SR_MAX_SEQNUM)
						next_seqnum = 0;
					while (!window.empty() && 
					window.begin()->seq <= next_seqnum) {
						if (window.begin()->seq == next_seqnum) {
							write(f, window.begin()->data, window.begin()->len);
							if ((next_seqnum += window.begin()->len) 
								> SR_MAX_SEQNUM)
								next_seqnum = 0;
						}
						window.pop_front();
					}
				} 
				else if (response.seq > next_seqnum
					&& response.seq < next_seqnum + SR_MAX_SEQNUM/2) {
					list<sr_packet>::iterator i = window.begin();
					while (i != window.end() && i->seq < response.seq)
						i++;
					if (i == window.end() || i->seq != response.seq) {
						ack_response(sockfd, serv_addr, response.seq, false, false);
						window.insert(i, response);  
					}
					else   
						ack_response(sockfd, serv_addr, response.seq, true, false);
				}
			} break;
			case SR_FIN: {
				if (response.seq == next_seqnum) {
					ack_response(sockfd, serv_addr, response.seq, false, true); 
					printf("* Finished request for \"%s\".\n", filename);
					printf("* Keeping socket open for a little bit.\n");
					struct timeval tv = {(2*SR_TIMEOUT)/1000, 0};  // 1 s
					if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, 
						sizeof(tv)) < 0)
						error("Error: setsockopt() failed");
				}
			} break;
		}
	}
	if (next_seqnum == initial_seqnum + 1)   
		goto retry;
	close(f);
}
