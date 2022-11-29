#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <list>

typedef int                     int32_t;
typedef long int                int64_t;
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
typedef unsigned long int       uint64_t;

class SocketRemoteInfo final {
public:
    SocketRemoteInfo();

    ~SocketRemoteInfo() = default;

    void SetAddress(const std::string &address);

    void SetFamily(sa_family_t family);

    void SetPort(uint16_t port);

    void SetSize(uint32_t size);

    const std::string &GetAddress() const;

    const std::string &GetFamily() const;

    uint16_t GetPort() const;

    uint32_t GetSize() const;

private:
    std::string address_;

    std::string family_;

    uint16_t port_;

    uint32_t size_;
};


class NetAddress final {
public:
    enum class Family : uint32_t {
        IPv4 = 1,
        IPv6 = 2,
    };

    NetAddress();

    ~NetAddress() = default;

    void SetAddress(std::string &address);

    void SetFamilyByJsValue(uint32_t family);

    void SetFamilyBySaFamily(sa_family_t family);

    void SetPort(uint16_t port);

    const std::string &GetAddress() const;

    uint32_t GetJsValueFamily() const;

    sa_family_t GetSaFamily() const;

    uint16_t GetPort() const;

private:
    std::string address_;

    Family family_;

    uint16_t port_;
};

