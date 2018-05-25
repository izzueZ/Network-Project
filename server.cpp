#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#define BUF_SIZE 1024 //bytes
#define MAX_PACKET_SIZE 1024 //bytes
#define HEADER_SIZE 8 //bytes
#define MAX_SEQ_NUM 30720 //bytes

int sockfd = -1;
string data = "";
unsigned long data_size = 0;
unsigned long data_offset = 0;

//read into data from specified file name
bool read_file(string filename) {
    ifstream file(filename.c_str());
    stringstream file_buffer;

    if (file) {
        file_buffer << file.rdbuf();
        data = file_buffer.str();
        return true;
    }

    return false;
}

int main(int argc, char *argv[])
{
    int portno;
    struct sockaddr_in serv_addr;
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

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

    char buffer[BUF_SIZE];

    while(1){
        int recv_len = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *) &src_addr, &addr_len);
        if (recv_len < 0) {
            fprintf(stderr,"FAIL to read from socket. ERROR: %s\n", strerror(errno));
            exit(1);
        }
        printf("%s\n", buffer);

        string filename = "";
        for (int i = 0; i < recv_len; i++)
            filename += buffer[i];

        if (read_file(filename)) {
            cout << data << endl;
            sendto(sockfd, data.c_str(), data.length()+1 , 0, (struct sockaddr*) &src_addr, addr_len);
        }
        else
            cout << "no data" << endl;
    }

    close(sockfd);

}
