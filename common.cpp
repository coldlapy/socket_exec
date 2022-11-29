#include "common.h"

SocketRemoteInfo::SocketRemoteInfo() : port_(0), size_(0) {}

void SocketRemoteInfo::SetAddress(const std::string &address)
{
    address_ = address;
}

void SocketRemoteInfo::SetFamily(sa_family_t family)
{
    if (family == AF_INET) {
        family_ = "IPv4";
    } else if (family == AF_INET6) {
        family_ = "IPv6";
    } else {
        family_ = "Others";
    }
}

void SocketRemoteInfo::SetPort(uint16_t port)
{
    port_ = port;
}

void SocketRemoteInfo::SetSize(uint32_t size)
{
    size_ = size;
}

const std::string &SocketRemoteInfo::GetAddress() const
{
    return address_;
}

const std::string &SocketRemoteInfo::GetFamily() const
{
    return family_;
}

uint16_t SocketRemoteInfo::GetPort() const
{
    return port_;
}

uint32_t SocketRemoteInfo::GetSize() const
{
    return size_;
}


NetAddress::NetAddress() : family_(Family::IPv4), port_(0) {}

void NetAddress::SetAddress(std::string &address)
{
    address_ = address;
}

void NetAddress::SetFamilyByJsValue(uint32_t family)
{
    if (static_cast<Family>(family) == Family::IPv6) {
        family_ = Family::IPv6;
    }
}

void NetAddress::SetFamilyBySaFamily(sa_family_t family)
{
    if (family == AF_INET6) {
        family_ = Family::IPv6;
    }
}

void NetAddress::SetPort(uint16_t port)
{
    port_ = port;
}

const std::string &NetAddress::GetAddress() const
{
    return address_;
}

sa_family_t NetAddress::GetSaFamily() const
{
    if (family_ == Family::IPv6) {
        return AF_INET6;
    }
    return AF_INET;
}

uint32_t NetAddress::GetJsValueFamily() const
{
    return static_cast<uint32_t>(family_);
}

uint16_t NetAddress::GetPort() const
{
    return port_;
}


