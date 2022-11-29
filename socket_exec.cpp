
#include "socket_exec.h"

static constexpr const int DEFAULT_BUFFER_SIZE = 8192;

static constexpr const int DEFAULT_POLL_TIMEOUT = 500; // 0.5 Seconds

static constexpr const int ADDRESS_INVALID = -1;

static constexpr const int NO_MEMORY = -2;


struct MessageData {
    MessageData() = delete;
    MessageData(void *d, size_t l, const SocketRemoteInfo &info) : data(d), len(l), remoteInfo(info) {}
    ~MessageData()
    {
        if (data) {
            free(data);
        }
    }

    void *data;
    size_t len;
    SocketRemoteInfo remoteInfo;
};

static std::string MakeAddressString(sockaddr *addr)
{
    if (addr->sa_family == AF_INET) {
        auto *addr4 = reinterpret_cast<sockaddr_in *>(addr);
        const char *str = inet_ntoa(addr4->sin_addr);
        if (str == nullptr || strlen(str) == 0) {
            return {};
        }
        return str;
    } else if (addr->sa_family == AF_INET6) {
        auto *addr6 = reinterpret_cast<sockaddr_in6 *>(addr);
        char str[INET6_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET6, &addr6->sin6_addr, str, INET6_ADDRSTRLEN) == nullptr || strlen(str) == 0) {
            return {};
        }
        return str;
    }
    return {};
}
static void OnRecvMessage(void *data, size_t len, sockaddr *addr)
{
    if (data == nullptr || len <= 0) {
        printf("OnRecvMessage nullptr or 0 \n");
        return;
    }

    SocketRemoteInfo remoteInfo;
    std::string address = MakeAddressString(addr);
    if (address.empty()) {
        printf("OnRecvMessage address empty \n");
        return;
    }
    remoteInfo.SetAddress(address);
    remoteInfo.SetFamily(addr->sa_family);
    if (addr->sa_family == AF_INET) {
        auto *addr4 = reinterpret_cast<sockaddr_in *>(addr);
        remoteInfo.SetPort(ntohs(addr4->sin_port));
    } else if (addr->sa_family == AF_INET6) {
        auto *addr6 = reinterpret_cast<sockaddr_in6 *>(addr);
        remoteInfo.SetPort(ntohs(addr6->sin6_port));
    }
    remoteInfo.SetSize(len);
}



class MessageCallback {
public:
    MessageCallback() {};

    virtual ~MessageCallback() {};

    virtual void OnMessage(int sock, void *data, size_t dataLen, sockaddr *addr) const = 0;
};

class TcpMessageCallback final : public MessageCallback {
public:
    TcpMessageCallback() {};

    ~TcpMessageCallback() {};

    void OnMessage(int sock, void *data, size_t dataLen, sockaddr *addr) const override
    {
        (void)addr;

        sa_family_t family;
        socklen_t len = sizeof(family);
        int ret = getsockname(sock, reinterpret_cast<sockaddr *>(&family), &len);
        if (ret < 0) {
            printf("OnMessage getsockname ret < 0 \n");
            return;
        }

        if (family == AF_INET) {
            sockaddr_in addr4 = {0};
            socklen_t len4 = sizeof(sockaddr_in);

            ret = getpeername(sock, reinterpret_cast<sockaddr *>(&addr4), &len4);
            if (ret < 0) {
                printf("OnMessage getpeername ret < 0 \n");
                return;
            }
            OnRecvMessage(data, dataLen, reinterpret_cast<sockaddr *>(&addr4));
            return;
        } else if (family == AF_INET6) {
            sockaddr_in6 addr6 = {0};
            socklen_t len6 = sizeof(sockaddr_in6);

            ret = getpeername(sock, reinterpret_cast<sockaddr *>(&addr6), &len6);
            if (ret < 0) {
                return;
            }
            OnRecvMessage(data, dataLen, reinterpret_cast<sockaddr *>(&addr6));
            return;
        }
    }
};

class UdpMessageCallback final : public MessageCallback {
public:
    UdpMessageCallback() {};

    ~UdpMessageCallback() {};

    void OnMessage(int sock, void *data, size_t dataLen, sockaddr *addr) const override
    {
        OnRecvMessage(data, dataLen, addr);
    }
};

static bool MakeNonBlock(int sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    while (flags == -1 && errno == EINTR) {
        flags = fcntl(sock, F_GETFL, 0);
    }
    if (flags == -1) {
        printf("make non block failed %s\n", strerror(errno));
        return false;
    }
    int ret = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    while (ret == -1 && errno == EINTR) {
        ret = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
    if (ret == -1) {
        printf("make non block failed %s\n", strerror(errno));
        return false;
    }
    return true;
}

static bool PollSendData(int sock, const char *data, size_t size, sockaddr *addr, socklen_t addrLen)
{
    int bufferSize = DEFAULT_BUFFER_SIZE;
    int opt = 0;
    socklen_t optLen = sizeof(opt);
    if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<void *>(&opt), &optLen) >= 0 && opt > 0) {
        bufferSize = opt;
    }
    int sockType = 0;
    optLen = sizeof(sockType);
    if (getsockopt(sock, SOL_SOCKET, SO_TYPE, reinterpret_cast<void *>(&sockType), &optLen) < 0) {
        printf("get sock opt sock type failed = %s\n", strerror(errno));
        return false;
    }

    auto curPos = data;
    auto leftSize = size;
    nfds_t num = 1;
    pollfd fds[1] = {{0}};
    fds[0].fd = sock;
    fds[0].events = 0;
    fds[0].events |= POLLOUT;

    while (leftSize > 0) {
        int ret = poll(fds, num, DEFAULT_POLL_TIMEOUT);
        if (ret == -1) {
            printf("poll to send failed %s\n", strerror(errno));
            return false;
        }
        if (ret == 0) {
            printf("poll to send timeout\n");
            return false;
        }

        size_t sendSize = (sockType == SOCK_STREAM ? leftSize : std::min<size_t>(leftSize, bufferSize));
        auto sendLen = sendto(sock, curPos, sendSize, 0, addr, addrLen);
        if (sendLen < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            printf("send failed %s\n", strerror(errno));
            return false;
        }
        if (sendLen == 0) {
            break;
        }
        curPos += sendLen;
        leftSize -= sendLen;
    }

    if (leftSize != 0) {
        printf("send not complete\n");
        return false;
    }
    return true;
}

static void PollRecvData(int sock, sockaddr *addr, socklen_t addrLen, bool isTcp, const MessageCallback &callback)
{
    int bufferSize = DEFAULT_BUFFER_SIZE;
    int opt = 0;
    socklen_t optLen = sizeof(opt);
    if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void *>(&opt), &optLen) >= 0 && opt > 0) {
        bufferSize = opt;
    }

    printf("PollRecvData bufferSize is %d\n", bufferSize);

    auto deleter = [](char *s) { free(reinterpret_cast<void *>(s)); };
    std::unique_ptr<char, decltype(deleter)> buf(reinterpret_cast<char *>(malloc(bufferSize)), deleter);

    auto addrDeleter = [](sockaddr *a) { free(reinterpret_cast<void *>(a)); };
    std::unique_ptr<sockaddr, decltype(addrDeleter)> pAddr(addr, addrDeleter);

    nfds_t num = 1;
    pollfd fds[1] = {{0}};
    fds[0].fd = sock;
    fds[0].events = 0;
    fds[0].events |= POLLIN;

    while (true) {
        int ret = poll(fds, num, DEFAULT_POLL_TIMEOUT);
        if (ret < 0) {
            printf("poll to recv failed %s\n", strerror(errno));

            return;
        }
        if (ret == 0) {
            continue;
        }
        (void)memset(buf.get(), 0, bufferSize);
        socklen_t tempAddrLen = addrLen;
        auto recvLen = recvfrom(sock, buf.get(), bufferSize, 0, addr, &tempAddrLen);
        if (recvLen < 0) {
            if (errno == EAGAIN) {
                printf("PollRecvData EAGAIN\n");
                continue;
            }
            printf("recv failed %s\n", strerror(errno));
            return;
        }
        if (recvLen == 0) {
            if (isTcp) {
                close(sock);
                pthread_exit(NULL);
            }
            continue;
        }

        void *data = malloc(recvLen);
        if (data == nullptr) {
            printf("PollRecvData data nullptr\n");
            return;
        }
        printf("copy ret = %d\n", memcpy(data, buf.get(), recvLen));
        printf("PollRecvData data %s, sock is %d\n", data, sock);
        callback.OnMessage(sock, data, recvLen, addr);
    }
}


static bool NonBlockConnect(int sock, sockaddr *addr, socklen_t addrLen, uint32_t timeoutSec)
{
    int ret = connect(sock, addr, addrLen);
    if (ret >= 0) {
        return true;
    }
    if (errno != EINPROGRESS) {
        printf("NonBlockConnect EINPROGRESS\n");
        return false;
    }

    fd_set set = {0};
    FD_ZERO(&set);
    FD_SET(sock, &set);
    if (timeoutSec == 0) {
        timeoutSec = 28800; // DEFAULT_CONNECT_TIMEOUT
    }
    timeval timeout = {
        .tv_sec = timeoutSec,
        .tv_usec = 0,
    };

    ret = select(sock + 1, nullptr, &set, nullptr, &timeout);
    if (ret < 0) {
        printf("select error: %s\n", strerror(errno));
        return false;
    } else if (ret == 0) {
        printf("timeout!\n");
        return false;
    }

    int err = 0;
    socklen_t optLen = sizeof(err);
    ret = getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<void *>(&err), &optLen);
    if (ret < 0) {
        return false;
    }
    if (err != 0) {
        return false;
    }
    return true;
}

static void GetAddr(NetAddress *address, sockaddr_in *addr4, sockaddr_in6 *addr6, sockaddr **addr, socklen_t *len)
{
    sa_family_t family = address->GetSaFamily();
    if (family == AF_INET) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(address->GetPort());
        addr4->sin_addr.s_addr = inet_addr(address->GetAddress().c_str());
        *addr = reinterpret_cast<sockaddr *>(addr4);
        *len = sizeof(sockaddr_in);
    } else if (family == AF_INET6) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(address->GetPort());
        inet_pton(AF_INET6, address->GetAddress().c_str(), &addr6->sin6_addr);
        *addr = reinterpret_cast<sockaddr *>(addr6);
        *len = sizeof(sockaddr_in6);
    }
}

int MakeTcpSocket(sa_family_t family)
{
    if (family != AF_INET && family != AF_INET6) {
        return -1;
    }
    int sock = socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        printf("make tcp socket failed errno is %d %s\n", errno, strerror(errno));
        return -1;
    }
    if (!MakeNonBlock(sock)) {
        close(sock);
        return -1;
    }
    return sock;
}

int MakeUdpSocket(sa_family_t family)
{
    if (family != AF_INET && family != AF_INET6) {
        return -1;
    }
    int sock = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        printf("make udp socket failed errno is %d %s\n", errno, strerror(errno));
        return -1;
    }
    if (!MakeNonBlock(sock)) {
        close(sock);
        return -1;
    }
    return sock;
}

bool ExecBind(int sockfd, NetAddress *address)
{
    sockaddr_in addr4 = {0};
    sockaddr_in6 addr6 = {0};
    sockaddr *addr = nullptr;
    socklen_t len;
    GetAddr(address, &addr4, &addr6, &addr, &len);
    if (addr == nullptr) {
        printf("addr family error\n");
        return false;
    }

    if (bind(sockfd, addr, len) < 0) {
        if (errno != EADDRINUSE) {
            printf("bind error is %s %d\n", strerror(errno), errno);
            return false;
        }
        if (addr->sa_family == AF_INET) {
            printf("distribute a random port\n");
            addr4.sin_port = 0; /* distribute a random port */
        } else if (addr->sa_family == AF_INET6) {
            printf("distribute a random port\n");
            addr6.sin6_port = 0; /* distribute a random port */
        }
        if (bind(sockfd, addr, len) < 0) {
            printf("rebind error is %s %d\n", strerror(errno), errno);
            return false;
        }
        printf("rebind success\n");
    }
    printf("bind successn");

    return true;
}

bool ExecUdpBind(int sockfd, NetAddress *address)
{
    if (!ExecBind(sockfd, address)) {
        return false;
    }

    sockaddr_in addr4 = {0};
    sockaddr_in6 addr6 = {0};
    sockaddr *addr = nullptr;
    socklen_t len;
    GetAddr(address, &addr4, &addr6, &addr, &len);
    if (addr == nullptr) {
        printf("addr family error\n");
        return false;
    }

    if (addr->sa_family == AF_INET) {
        auto pAddr4 = reinterpret_cast<sockaddr *>(malloc(sizeof(addr4)));
        printf("copy ret = %d\n", memcpy(pAddr4, &addr4, sizeof(addr4)));
        std::thread serviceThread(PollRecvData, sockfd, pAddr4, sizeof(addr4), false,
                                  UdpMessageCallback());
        serviceThread.detach();
    } else if (addr->sa_family == AF_INET6) {
        auto pAddr6 = reinterpret_cast<sockaddr *>(malloc(sizeof(addr6)));
        printf("copy ret = %d\n", memcpy(pAddr6,  &addr6, sizeof(addr6)));
        std::thread serviceThread(PollRecvData, sockfd, pAddr6, sizeof(addr6), false,
                                  UdpMessageCallback());
        serviceThread.detach();
    }

    return true;
}

bool ExecUdpSend(int sockfd, NetAddress *address, std::string send_str, socklen_t size)
{
    sockaddr_in addr4 = {0};
    sockaddr_in6 addr6 = {0};
    sockaddr *addr = nullptr;
    socklen_t len;
    GetAddr(address, &addr4, &addr6, &addr, &len);
    if (addr == nullptr) {
        printf("addr family error\n");
        return false;
    }

    if (!PollSendData(sockfd, send_str.c_str(), size, addr, len)) {
        return false;
    }
    return true;
}

bool ExecTcpBind(int sockfd, NetAddress *address)
{
    return ExecBind(sockfd, address);
}

bool ExecConnect(int sockfd, NetAddress *address, uint32_t timeoutSec)
{
    sockaddr_in addr4 = {0};
    sockaddr_in6 addr6 = {0};
    sockaddr *addr = nullptr;
    socklen_t len;
    GetAddr(address, &addr4, &addr6, &addr, &len);
    if (addr == nullptr) {
        printf("addr family error\n");
        return false;
    }

    if (!NonBlockConnect(sockfd, addr, len, timeoutSec)) {
        printf("connect errno %d %s\n", errno, strerror(errno));
        return false;
    }

    printf("connect success\n");
    std::thread serviceThread(PollRecvData, sockfd, nullptr, 0, true,
                              TcpMessageCallback());
    serviceThread.detach();
    return true;
}

bool ExecTcpSend(int sockfd, std::string send_str, socklen_t size) 
{
    sa_family_t family;
    socklen_t len = sizeof(sa_family_t);
    if (getsockname(sockfd, reinterpret_cast<sockaddr *>(&family), &len) < 0) {
        printf("get sock name failed\n");
        return false;
    }
    bool connected = false;
    if (family == AF_INET) {
        sockaddr_in addr4 = {0};
        socklen_t len4 = sizeof(addr4);
        int ret = getpeername(sockfd, reinterpret_cast<sockaddr *>(&addr4), &len4);
        if (ret >= 0 && addr4.sin_port != 0) {
            connected = true;
        }
    } else if (family == AF_INET6) {
        sockaddr_in6 addr6 = {0};
        socklen_t len6 = sizeof(addr6);
        int ret = getpeername(sockfd, reinterpret_cast<sockaddr *>(&addr6), &len6);
        if (ret >= 0 && addr6.sin6_port != 0) {
            connected = true;
        }
    }

    if (!connected) {
        printf("sock is not connect to remote %s\n", strerror(errno));
        return false;
    }

    if (!PollSendData(sockfd, send_str.c_str(), size, nullptr, 0)) {
        printf("send errno %d %s\n", errno, strerror(errno));
        return false;
    }
    return true;
}

static void TcpListenConn(int socketFd)
{
    while(true) {
        int connfd = accept(socketFd, NULL, NULL);
        if (connfd == -1) {
            if (errno == EWOULDBLOCK) {
                sleep(1);
            } else {
                printf("error when accepting connection errno  %{public}d  %{public}s\n",
                    errno, strerror(errno));
                return;
            }
        } else {
            printf("accept new connfd is %{public}d", connfd);
            MakeNonBlock(connfd);
            std::thread serviceThread(PollRecvData, connfd, nullptr, 0, true,
                                      TcpMessageCallback());
            serviceThread.detach();
        }
    }
}

bool ExecTcpListen(int sockfd)
{
    printf("tcp server start listen.");
    int ret = listen(sockfd, 10);
    if (ret < 0) {
        printf("listen errno %{public}d %{public}s", errno, strerror(errno));
        return false;
    }

    std::thread serviceThread(TcpListenConn, sockfd);
    serviceThread.detach();
    printf("the tcp listening server has been started.");
    return true;
}

