#include "packet.h"

int sockfd = -1;

void check_time(int sockfd, struct sockaddr_in& cli_addr, list<packet>& window){
    for(list<packet>::iterator i = window.begin(); i != window.end(); i++) {
        if(timestamp() > i->timestamp + TIMEOUT) {
            //printf("- Timeout!\n");
            i->timestamp = timestamp();
            send_packet(sockfd, cli_addr, *i, true);
        }
    }
}

void process_ack(int sockfd, struct sockaddr_in& cli_addr, list<packet>& window, bool done){
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, NULL) | O_NONBLOCK);
    
    struct packet client_ack;
    while(recvfrom(sockfd, &client_ack, sizeof(client_ack), 0, (struct sockaddr *) &cli_addr, &addr_len) > 0) {
        printf("Receiving packet %d\n", client_ack.ack);
        if(client_ack.type != type_ACK && client_ack.type != (type_ACK + type_SYN) && client_ack.type != (type_ACK + type_ERR) && client_ack.type != (type_ACK + type_FIN)){
            printf("----- Received packet, but is not expected one. Ignore it. -----\n");
            continue;
        }
        
        //printf("Receiving packet %d\n", client_ack.ack);
        
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
                printf("----- Unknown situation -----\n");
            }
        }
        /*if(client_ack.ack == 0){
            list<packet>::iterator j = window.begin();
            if(MAX_SEQ_NUM - j->seq < PAYLOAD_SIZE){
                j = window.erase(j);
            }
            else{
                j++;
            }
        }
        
        
        if(done){
            i = window.end(); //?????
            if(i->seq < (client_ack.ack - PAYLOAD_SIZE)){
                window.pop_back();
            }
        }
        while(i != window.end() && i->seq != (client_ack.ack - PAYLOAD_SIZE)){
            i++;
        }
        //received ack from client for a packet in the window
        if(i != window.end()){
            i = window.erase(i);
            duplicate_num = 0;
        }
        else{
            duplicate_num++;
            if(duplicate_num == 3){
                ;
            }
        }*/
    }
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, NULL) & ~O_NONBLOCK);
}

void process_request(int sockfd, struct sockaddr_in& cli_addr, const struct packet& request)
{
    //int count = 0;
    list<packet> window;
    bool done_flag = false;
	int16_t seq_num = randint(0, MAX_SEQ_NUM/2); //y
    int16_t ack_num = request.seq + request.len;
    printf("Receiving packet %d\n", request.ack); //should be "Receiving packet -1"
	struct packet syn_ack = {
	   	timestamp(),
	    type_ACK + type_SYN,
	    1,
	    (int16_t) seq_num,
	    (int16_t) ack_num
    };
    //printf("this is server SYN&ACK to client as step 2 in line 70.\n");
    send_packet(sockfd, cli_addr, syn_ack, false);
    window.push_back(syn_ack);
    
    //struct packet file_request;
    /*while(!window.empty()) {
        if(recvfrom(sockfd, &file_request, sizeof(file_request), 0, (struct sockaddr*)&cli_addr, &addr_len) > 0) {
    	    //printf("- Waiting for ACK&REQ.\n");
            if(file_request.type != (type_ACK + type_REQ) || file_request.ack != seq_num + 1) {
                printf("- ACK&REQ from client is wrong. Three-way handshake failed.\n");
                return;
            }
            else{
                window.pop_back();
                printf("- Received SYN&REQ from client. Start sending DATA.\n");
            }
        }
        else {
            check_time(sockfd, cli_addr, window);
        }
    }*/
    while(!window.empty()) {
        process_ack(sockfd, cli_addr, window, false);
        check_time(sockfd, cli_addr, window);
    }
    seq_num += syn_ack.len;
    ////////////////// Three-way handshake complete//////////////

    //ack_num += file_request.len;
    int f = open((char*) &request.data, O_RDONLY);
	if (f < 0) {
        const char msg[] = "404 File Not Found";
	    struct packet error_packet = {
            timestamp(),
            type_ERR,
            (int16_t) strlen(msg),
            (int16_t) seq_num,
            -2
        };
        strncpy((char*)error_packet.data, msg, strlen(msg));
        send_packet(sockfd, cli_addr, error_packet, false);
        window.push_back(error_packet);
        //struct packet error_ack;
        /*while(recvfrom(sockfd, &error_ack, sizeof(error_ack), 0, (struct sockaddr*)&cli_addr, &addr_len) < 0) {
            printf("- Waiting for ERR&ACK.\n");
            check_time(sockfd, cli_addr, window);
        }
        if(error_ack.type != (type_ACK + type_ERR)) {
            printf("- ACK&ERR from client is wrong.\n");
            return;
        }
        else{
            printf("- Received ACK&ERR from client.\n");
        }*/
        /*while(!window.empty()) {
            if(recvfrom(sockfd, &error_ack, sizeof(error_ack), 0, (struct sockaddr*)&cli_addr, &addr_len) < 0) {
                printf("- Waiting for ERR&ACK.\n");
                check_time(sockfd, cli_addr, window);
            }
            else if(error_ack.type != (type_ACK + type_ERR)) {
                printf("- ACK&ERR from client is wrong.\n");
                return;
            }
            else {
                window.pop_back();
                printf("- Received ACK&ERR from client.\n");
            }
        }*/
        while(!window.empty()) {
            process_ack(sockfd, cli_addr, window, false);
            check_time(sockfd, cli_addr, window);
        }
        seq_num += error_packet.len;
	}
    else while(!window.empty() || !done_flag){
        //while(!done_flag && (window.empty() || (window.begin()->seq + CWND >= seq_num + MAX_PACKET_SIZE)) && window.size() < 5){
        while(!done_flag && window.size() < 5){
            struct packet data_packet = {
                timestamp(),
                type_DATA,
                0,
                (int16_t) seq_num,
                -3
            };
            
            if((data_packet.len = read(f, data_packet.data, PAYLOAD_SIZE)) > 0){
                //printf("- Sending DATA.\n");
                send_packet(sockfd, cli_addr, data_packet, false);
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
        
        /*if(done_flag){
            process_ack(sockfd, cli_addr, window, true);
        }
        else{
            process_ack(sockfd, cli_addr, window, false);
        }*/
        process_ack(sockfd, cli_addr, window, false);
        check_time(sockfd, cli_addr, window);
    }
    
    /////////////////////////////////////////////////////////////////////////////////
    struct packet fin_packet = {
        timestamp(),
        type_FIN,
        1,
        (int16_t) seq_num,
        -4
    };
    //printf("- Sending FIN.\n");
    send_packet(sockfd, cli_addr, fin_packet, false);
    window.push_back(fin_packet);

    seq_num += fin_packet.len; //@
    //struct packet fin_ack;
    /*while(recvfrom(sockfd, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr *) &cli_addr, &addr_len) < 0) {
        printf("- Waiting for FIN&ACK.\n");
        check_time(sockfd, cli_addr, window);
    }
    if(fin_ack.type != (type_ACK + type_FIN) or fin_ack.ack != seq_num) {
        printf("- FIN&ACK is wrong.\n");
        return;
    }
    else{
        printf("received ACK from client for FIN.\n");
        printf("file transfer complete.\n");
    }*/
    /*while(!window.empty()) {
        if(recvfrom(sockfd, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr *) &cli_addr, &addr_len) < 0) {
            printf("- Waiting for FIN&ACK.\n");
            check_time(sockfd, cli_addr, window);
        }
        else if (fin_ack.type != (type_ACK + type_FIN) or fin_ack.ack != seq_num) {
            printf("- FIN&ACK is wrong.\n");
            return;
        } 
        else {
            window.pop_back();
            printf("received ACK from client for FIN.\n");
            printf("file transfer complete.\n");
        }  
    }*/
    while(!window.empty()) {
        process_ack(sockfd, cli_addr, window, false);
        check_time(sockfd, cli_addr, window);
    }
    //printf("file transfer complete.\n");
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
        	printf("- Warning: Unrecognized packet.\n");
    }
    close(sockfd); //@
}
