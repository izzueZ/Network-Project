#include "packet_cc.h"

int sockfd = -1;
int16_t ack_num = MAX_SEQ_NUM + 1;

void check_time(int sockfd, struct sockaddr_in& cli_addr, list<packet>& window){
    for(list<packet>::iterator i = window.begin(); i != window.end(); i++) {
        if(timestamp() > i->timestamp + TIMEOUT) {
            i->timestamp = timestamp();
            send_packet(sockfd, cli_addr, *i, true, true);
            ssthresh = max(CWND/2, MAX_PACKET_SIZE*2);
            CWND = MAX_PACKET_SIZE;
            mode = SS;
            duplicate_num = 0;
        }
    }
}

void process_ack(int sockfd, struct sockaddr_in& cli_addr, list<packet>& window){
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, NULL) | O_NONBLOCK);
    
    struct packet client_ack;
    while(recvfrom(sockfd, &client_ack, sizeof(client_ack), 0, (struct sockaddr *) &cli_addr, &addr_len) > 0) {
        if(client_ack.type != type_ACK && client_ack.type != (type_ACK + type_SYN) && client_ack.type != (type_ACK + type_ERR) && client_ack.type != (type_ACK + type_FIN)){
            printf("----- Received packet, but it's not expected one. Ignore it. -----\n");
            continue;
        }
    
        if(window.empty()) { //received dup ack
            switch (mode) {
                case SS: 
                case CA:
                    //printf("----- Duplicate ACK. -----\n");
                    if(++duplicate_num == 3) {
                        ssthresh = CWND/2;
                        CWND = min(ssthresh + 3*MAX_PACKET_SIZE, MAX_SEQ_NUM/2);
                        mode = FR;
                    }
                    break;
                case FR:
                    CWND = min(CWND + MAX_PACKET_SIZE, MAX_SEQ_NUM/2);
                    break;
            }
            return;
        }
        list<packet>::iterator i = window.begin();
        while(i != window.end() && i->seq != client_ack.ack)
            i++;
        if (i != window.end()) {
            for(list<packet>::iterator j = window.begin(); j != i;)
                j = window.erase(j);
        }
        else {
            if((window.back().seq + window.back().len) % MAX_SEQ_NUM == client_ack.ack) {
                window.clear();
            }
            else {
                printf("----- Unknown situation. -----\n");
            }
        }
        switch(mode) {
            case SS:
                //printf("----- Mode SS. -----\n");
                CWND = min(CWND + MAX_PACKET_SIZE, MAX_SEQ_NUM/2);
                duplicate_num = 0;
                if(CWND >= ssthresh) {
                    //printf("----- Switch from SS to CA. -----\n");
                    mode = CA;
                }
                break;
            case CA:
                //printf("----- Mode CA. -----\n");
                CWND = min(CWND + (MAX_PACKET_SIZE/CWND) * MAX_PACKET_SIZE, MAX_SEQ_NUM/2);
                duplicate_num = 0;
                break;
            case FR:
                CWND = min(ssthresh, MAX_SEQ_NUM/2);
                duplicate_num = 0;
                mode = CA;
                break;
        }
        ack_num = client_ack.seq + 1;
        printf("Receiving packet %d\n\n", ack_num);
    }
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, NULL) & ~O_NONBLOCK);
}

void process_request(int sockfd, struct sockaddr_in& cli_addr, const struct packet& request)
{
    list<packet> window;
    bool done_flag = false;
	int16_t seq_num = randint(0, MAX_SEQ_NUM/2); //y
    ack_num = request.seq + request.len;
    printf("Receiving packet %d\n\n", ack_num); //should be "Receiving packet -1"
	struct packet syn_ack = {
	   	timestamp(),
	    type_ACK + type_SYN,
	    1,
	    (int16_t) seq_num,
	    (int16_t) ack_num
    };
    send_packet(sockfd, cli_addr, syn_ack, false, true);
    window.push_back(syn_ack);

    while(!window.empty()) {
        process_ack(sockfd, cli_addr, window);
        check_time(sockfd, cli_addr, window);
    }
    seq_num += syn_ack.len;
    ////////////////// Three-way handshake complete//////////////

    int f = open((char*) &request.data, O_RDONLY);
	if (f < 0) {
        const char msg[] = "404 File Not Found";
	    struct packet error_packet = {
            timestamp(),
            type_ERR,
            (int16_t) strlen(msg),
            (int16_t) seq_num,
            (int16_t) ack_num
        };
        strncpy((char*)error_packet.data, msg, strlen(msg));
        send_packet(sockfd, cli_addr, error_packet, false, true);
        window.push_back(error_packet);

        while(!window.empty()) {
            process_ack(sockfd, cli_addr, window);
            check_time(sockfd, cli_addr, window);
        }
        seq_num += error_packet.len;
	}
    else while(!window.empty() || !done_flag){
        while(!done_flag && window.size() < CWND/1024){ //edit
            struct packet data_packet = {
                timestamp(),
                type_DATA,
                0,
                (int16_t) seq_num,
                (int16_t) ack_num
            };
            
            if((data_packet.len = read(f, data_packet.data, PAYLOAD_SIZE)) > 0){
                send_packet(sockfd, cli_addr, data_packet, false, true);
                window.push_back(data_packet);
                seq_num += data_packet.len;
                if(seq_num > MAX_SEQ_NUM){
                    seq_num = seq_num % MAX_SEQ_NUM;
                }
            }
            else{
                done_flag = true;
            }
        }
        
        process_ack(sockfd, cli_addr, window);
        check_time(sockfd, cli_addr, window);
    }
    
    /////////////////////////////////////////////////////////////////////////////////
    struct packet fin_packet = {
        timestamp(),
        type_FIN,
        1,
        (int16_t) seq_num,
        (int16_t) ack_num
    };
    send_packet(sockfd, cli_addr, fin_packet, false, true);
    window.push_back(fin_packet);

    seq_num += fin_packet.len; //@
    while(!window.empty()) {
        process_ack(sockfd, cli_addr, window);
        check_time(sockfd, cli_addr, window);
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
        	printf("----- Start processing new request. -----\n\n");
        	process_request(sockfd, cli_addr, request);
            printf("----- Finish request. -----\n\n");
        }
        else 
        	printf("----- Warning: Unrecognized packet. ----\n");
    }
    close(sockfd); //@
}
