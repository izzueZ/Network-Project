#include "shared.h"
 

void send_fin(int sockfd, struct sockaddr_in& cli_addr, 
	list<sr_packet>& window, int16_t next_seqnum) {
	struct sr_packet fin = { 
		timestamp(),
		SR_FIN, 
		1,   
		next_seqnum,   
		-2   
	};
	send_packet(sockfd, cli_addr, fin, false);
	window.push_back(fin);
}



int listen_to_udp(int portno) {
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("Error: failed creating a socket");
	 
	const int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
		(const void*)&optval, sizeof(optval));
	
	struct sockaddr_in serv_addr = generate_serv_addr(portno, NULL);
	if (bind(sockfd, (struct sockaddr*)&serv_addr, addrlen) < 0)
		error("Error: failed binding");
	return sockfd;
}
 
void send_error(int sockfd, struct sockaddr_in& cli_addr, 
	list<sr_packet>& window, int16_t next_seqnum) {
	const char msg[] = "404 File not found";
	struct sr_packet response = { 
		timestamp(),
		SR_STATUS,
		(int16_t)strlen(msg),  
		next_seqnum,   
		-2   
	};
	strncpy((char*)response.data, msg, strlen(msg));
	send_packet(sockfd, cli_addr, response, false);
	window.push_back(response);
}

void process_acks(int sockfd, struct sockaddr_in& cli_addr, 
	list<sr_packet>& window) {
	fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, NULL) | O_NONBLOCK); 
	struct sr_packet ack;
	while (recvfrom(sockfd, &ack, sizeof(ack), 0, 
		(struct sockaddr*)&cli_addr, &addrlen) > 0) {
		if (ack.type != SR_ACK) {
			printf("Receiving packet %d\n", ack.seq);
			printf("Warning: Unexpected packet ignored.\n");
			continue;
		}
		printf("Receiving packet %d\n", ack.ack);
		list<sr_packet>::iterator i = window.begin();
		while (i != window.end() && i->seq != ack.ack) 
			i++;
		if (i != window.end()) {   
			i = window.erase(i);
			switch (mode) {
				case SR_SS: 
					set_cwnd(cwnd + SR_PKT_SIZE);
					dupackcount = 0;
					break;
				case SR_CA: 
					set_cwnd(cwnd + SR_PKT_SIZE*(SR_PKT_SIZE/cwnd));
					break;
				case SR_FR:
					set_cwnd(ssthresh);
					dupackcount = 0;
					mode = SR_CA;
					break;
			}
		}
		else switch (mode) { 
			case SR_SS:
			case SR_CA:
				if (++dupackcount == 3) {
					ssthresh = cwnd/2;
					set_cwnd(ssthresh + 3*SR_PKT_SIZE);
					mode = SR_FR;
				}
				break;
			case SR_FR:
				set_cwnd(cwnd + SR_PKT_SIZE);
				break;
		}
	}
	fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, NULL) & ~O_NONBLOCK);
}

void time_checking(int sockfd, struct sockaddr_in& cli_addr, 
	list<sr_packet>& window) {
	for (list<sr_packet>::iterator i = window.begin(); i != window.end(); i++)
		if (timestamp() > i->timestamp + SR_TIMEOUT) {
			i->timestamp = timestamp();
			send_packet(sockfd, cli_addr, *i, true);
			{   
				ssthresh = max(window.size(), 2*SR_PKT_SIZE); 
				set_cwnd(SR_PKT_SIZE);
				dupackcount = 0;
				mode = SR_SS;
			}
		}
}

void handle_client(int sockfd, struct sockaddr_in& cli_addr,
	const struct sr_packet& request) {
	 
	struct hostent* hostp = gethostbyaddr(
		(const char*)&cli_addr.sin_addr.s_addr, 
		sizeof(cli_addr.sin_addr.s_addr), AF_INET);
	if (hostp == NULL) {
		fprintf(stderr, "Error: get host by address failed\n");
		exit(1);
	}
	const char* hostaddrp = inet_ntoa(cli_addr.sin_addr);
	if (hostaddrp == NULL) {
		fprintf(stderr, "Error: inet_ntoa() failed\n");
		exit(1);
	}
		 
	printf("* Processing request from %s (%s) for \"%s\".\n", 
		hostp->h_name, hostaddrp, request.data);
	printf("Receiving packet %d\n", request.seq);
	list<sr_packet> window;
	struct sr_packet syn_echo = {
		timestamp(),
		SR_SYN,
		1,   
		(int16_t)(request.seq + 1),   
		-2   
	};
    printf("this is the line for syn_echo.\n");
	send_packet(sockfd, cli_addr, syn_echo, false);
    window.push_back(syn_echo);
    printf("the window size is %lu.\n", window.size());
    //ack from client after receiving server's syn ack --> 3rd step
	while (!window.empty()) {
		process_acks(sockfd, cli_addr, window);
		time_checking(sockfd, cli_addr, window);
	} 
	int f = open((char*)request.data, O_RDONLY);
	int16_t next_seqnum = syn_echo.seq + 1;
	bool done = false;  
	if (f == -1) {
		send_error(sockfd, cli_addr, window, next_seqnum++);
		while (!window.empty()) {
			process_acks(sockfd, cli_addr, window);
			time_checking(sockfd, cli_addr, window);
		}
	}
	else while (!done || !window.empty()) { 
		while (!done && (window.empty() || (window.begin()->seq + cwnd >= 
			next_seqnum + SR_PKT_SIZE && next_seqnum != 0))) {
			struct sr_packet response = { 
				timestamp(),
				SR_DATA, 
				0,   
				(int16_t)next_seqnum,   
				-2   
			};
			if ((response.len = read(f, response.data, SR_MAX_LEN)) > 0) {
				send_packet(sockfd, cli_addr, response, false);
				window.push_back(response);
				 
				if ((next_seqnum += response.len) > SR_MAX_SEQNUM)
					next_seqnum = 0;
			}
			else
				done = true;
		}
		process_acks(sockfd, cli_addr, window);
		time_checking(sockfd, cli_addr, window);
	}
	
	send_fin(sockfd, cli_addr, window, next_seqnum++);
	while (!window.empty()) {
		process_acks(sockfd, cli_addr, window);
		time_checking(sockfd, cli_addr, window);
	}
	printf("* Done processing request from %s (%s) for \"%s\".\n", 
		hostp->h_name, hostaddrp, request.data);
}
 
int main(int argc, char* argv[]) { 
	if (argc == 3 && strcmp(argv[2], "diag") == 0)
		diag = true;
	else if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	int portno = atoi(argv[1]);
	if (portno < 0 || portno > 65535) {
		fprintf(stderr, "%d is not a valid port number.\n", portno);
		exit(1);
	}
	 
	int sockfd = listen_to_udp(portno);
	while (true) {
        printf("this is the line at 205.\n");
		struct sr_packet request;
		struct sockaddr_in cli_addr;
		if (recvfrom(sockfd, &request, sizeof(request), 0, 
			(struct sockaddr*)&cli_addr, &addrlen) < 0)
			error("Error: recvfrom() failed");
        printf("this is the request data: %s.\n", request.data);
        if (request.type == SR_SYN){
            printf("this is the line for SYN.\n");
			handle_client(sockfd, cli_addr, request);
        }
		else
			printf("Warning: Unexpected packet ignored.\n");
	}
}
