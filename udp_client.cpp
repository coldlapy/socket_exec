#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "socket_exec.h"

static std::string CLIENT_IP = "10.100.91.65";
static  uint16_t CLIENT_PORT = 7777;

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
    int sockfd = MakeUdpSocket(family);

    NetAddress srv_addr;
    srv_addr.SetAddress(ip);
    srv_addr.SetPort(port);

    NetAddress client_addr;
    client_addr.SetAddress(CLIENT_IP);
    client_addr.SetPort(CLIENT_PORT);

    ExecUdpBind(sockfd, &client_addr);

    std::string send_str = "hello 1234";
    socklen_t size = strlen(send_str.c_str());
    ExecUdpSend(sockfd, &srv_addr, send_str, size);

    sleep(100);

    return 0;
}
