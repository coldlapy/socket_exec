#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "socket_exec.h"

int main(int argc, char **argv)
{
    if(argc < 3) {
        printf("Less no of arguments !!");
        return 0;
    }

    std::string ip(argv[1]);
    uint16_t port = atoi(argv[2]);

    printf("ip is %s, port is %d !!\n", ip.c_str(), port);

    sa_family_t family = AF_INET;
    int sockfd = MakeTcpSocket(family);

    NetAddress srv_addr;
    srv_addr.SetAddress(ip);
    srv_addr.SetPort(port);

    ExecTcpBind(sockfd, &srv_addr);

    ExecTcpListen(sockfd);

    sleep(100);

    return 0;
}
