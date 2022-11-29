
#include "common.h"

int MakeTcpSocket(sa_family_t family);

int MakeUdpSocket(sa_family_t family);

bool ExecUdpBind(int sockfd, NetAddress *address);

bool ExecUdpSend(int sockfd, NetAddress *address, std::string send_str, socklen_t size);

bool ExecTcpBind(int sockfd, NetAddress *address);

bool ExecConnect(int sockfd, NetAddress *address, uint32_t timeoutSec);

bool ExecTcpSend(int sockfd, std::string send_str, socklen_t size);

bool ExecTcpListen(int sockfd);

