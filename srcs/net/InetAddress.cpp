// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
#include <array>

#include "net/InetAddress.h"
#include "net/Socketsops.h"
#include "net/Endian.h"

// #include "muduo/net/SocketsOps.h"

#include <cassert>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
using std::string;

static const in_addr_t kInaddrAny      = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };


InetAddress::InetAddress(uint16_t port, bool loopback_only, bool ipv6)
{
    if (ipv6)
    {
        // memZero(&addr6_, sizeof addr6_);
        struct sockaddr_in6 addr6
        {
        };
        addr6.sin6_family = AF_INET6;
        in6_addr ip       = loopback_only ? in6addr_loopback : in6addr_any;
        addr6.sin6_addr   = ip;
        addr6.sin6_port   = Sock::HostToNetwork16(port);
        addr_             = addr6;
    }
    else
    {
        // memZero(&addr_, sizeof addr_);
        struct sockaddr_in addr
        {
        };
        addr.sin_family      = AF_INET;
        auto ip         = loopback_only ? kInaddrLoopback : kInaddrAny;
        addr.sin_addr.s_addr = Sock::HostToNetwork32(ip);
        addr.sin_port        = Sock::HostToNetwork16(port);
        addr_                = addr;
    }
}

InetAddress::InetAddress(std::string ip, uint16_t port, bool ipv6)
{
    if (ipv6 || (strchr(ip.c_str(), ':') != nullptr))
    {
        addr_ = sockaddr_in6 {};
        Sock::FromIpPort(ip.c_str(), port, &std::get<1>(addr_));
    }
    else
    {
        addr_ = sockaddr_in {};
        Sock::FromIpPort(ip.c_str(), port, &std::get<0>(addr_));
    }
}

auto InetAddress::toIpPortRepr() const
    -> std::string
{
    auto buf = std::array<char, 256> {};
    buf.fill('\0');
    Sock::toIpPortRepr(buf.data(), buf.size(), getSockAddr());
    return std::string(buf.data());
}

auto InetAddress::toIpRepr() const
    -> std::string
{
    auto buf = std::string(64, '\0');
    Sock::toIp(buf.data(), buf.length(), getSockAddr());
    return buf;
}

auto InetAddress::Ipv4NetEndian() const
    -> uint32_t
{
    assert(getFamily() == AF_INET);
    return std::get<0>(addr_).sin_addr.s_addr;
}

auto InetAddress::GetPort() const
    -> uint16_t
{
    return Sock::networkToHost16(PortNetEndian());
}

static thread_local char t_resolveBuffer[64 * 1024];

auto InetAddress::Resolve(std::string hostname, InetAddress* result)
    -> bool
{
    assert(result != nullptr);
    struct hostent* he = nullptr;
    int herrno         = 0;
    struct hostent hent{};
    // memZero(&hent, sizeof(hent));

    int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
    if (ret == 0 && he != NULL)
    {
        assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
        std::get<0>(result->addr_).sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
        return true;
    }
    else
    {
        if (ret)
        {
            // LOG_SYSERR << "InetAddress::resolve";
        }
        return false;
    }
}

void InetAddress::SetScopeId(uint32_t scope_id)
{
    if (getFamily() == AF_INET6)
    {
        std::get<1>(addr_).sin6_scope_id = scope_id;
    }
}

auto InetAddress::getSockAddr() const
    -> const struct sockaddr*
{
    if (addr_.index() == 0)
        return Sock::sockaddrCast<sockaddr>(&std::get<sockaddr_in>(addr_));

    return Sock::sockaddrCast<sockaddr>(&std::get<sockaddr_in6>(addr_));
}

auto InetAddress::GetLocalInetAddress(int sockfd)
    -> InetAddress
{

    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    auto localaddr = Sock::GetLocalAddr(sockfd);
    return InetAddress {Sock::sockaddrCast<sockaddr>(&localaddr)};
}

auto InetAddress::GetPeerInetAddress(int sockfd)
    -> InetAddress
{
    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    auto localaddr = Sock::GetLocalAddr(sockfd);
    return InetAddress {Sock::sockaddrCast<sockaddr>(&localaddr)};
}