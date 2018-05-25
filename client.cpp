#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <errno.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

#define BUF_SIZE 1024 //bytes

string filename = "";
int sockfd = -1;

int main(int argc, char *argv[])
{
    int portno;
    struct sockaddr_in serv_addr;
    struct sockaddr_in src_addr;
    socklen_t serv_len;
    socklen_t addr_len;
    struct hostent *server;
    char* hostname;

    if (argc < 4) {
        fprintf(stderr, "ERROR, no port or hostname or request provided\n");
        exit(1);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create socket
    if (sockfd < 0) {
        fprintf(stderr, "ERROR opening server socket\n");
        exit(1);
    }

    server = gethostbyname(hostname);

    if (!server) {
        fprintf(stderr, "ERROR obtaining host address of %s\n", hostname);
        exit(1);
    }

    memset((char*) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy((char*) &serv_addr.sin_addr, server->h_addr, server->h_length);
    serv_len = sizeof(serv_addr);

    char* message = new char[filename.length()+1];
    strcpy(message, filename.c_str());

    int send_len = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &serv_addr, serv_len);
    if (send_len < 0) {
        fprintf(stderr,"FAIL to read from socket. ERROR: %s\n", strerror(errno));
        exit(1);
    }

    char buffer[BUF_SIZE];

    while(1) {
    	int recv_len = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *) &src_addr, &addr_len);
        if (recv_len < 0) {
            fprintf(stderr,"FAIL to read from socket. ERROR: %s\n", strerror(errno));
            exit(1);
        }
        printf("%s\n", buffer);
    }

    close(sockfd);
}
