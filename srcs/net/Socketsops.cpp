// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <cassert>
#include <cerrno>
#include <cstdio> // snprintf
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/uio.h> // readv
#include <unistd.h>

#include "net/Socketsops.h"
#include "net/Endian.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"

static auto log = GET_ROOT_LOGGER();

namespace {

using SA = struct sockaddr;

} // namespace

namespace Sock {
auto createNonblockingOrDie(sa_family_t family)
    -> int
{
    auto sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG<LogLevel::SYSFATAL>(log, "createNonblockingOrDie");
        // LOG_SYSFATAL << "createNonblockingOrDie";
    }
    return sockfd;
}

void BindOrDie(int sockfd, const struct sockaddr* addr)
{
    auto ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
    if (ret < 0)
    {
        // todo: log and exit
        //  LOG_SYSFATAL << "bindOrDie";
    }
}

void ListenOrDie(int sockfd)
{
    auto ret = ::listen(sockfd, SOMAXCONN);
    if (ret < 0)
    {
        // LOG_SYSFATAL << "listenOrDie";
    }
}

auto Accept(int sockfd, struct sockaddr_in6* addr)
    -> int
{
    auto addrlen = static_cast<socklen_t>(sizeof *addr);
    int connfd   = ::accept4(sockfd, sockaddrCast<sockaddr>(addr), &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd < 0)
    {
        auto saved_errno = errno;
        // LOG_SYSERR << "Socket::accept";
        switch (saved_errno)
        {
            case EAGAIN:
            case ECONNABORTED:
            case EINTR:
            case EPROTO: // ???
            case EPERM:
            case EMFILE: // per-process lmit of open file desctiptor ???
                // expected errors
                errno = saved_errno;
                break;
            case EBADF:
            case EFAULT:
            case EINVAL:
            case ENFILE:
            case ENOBUFS:
            case ENOMEM:
            case ENOTSOCK:
            case EOPNOTSUPP:
                // unexpected errors
                // LOG_FATAL << "unexpected error of ::accept " << savedErrno;
                break;
            default:
                // LOG_FATAL << "unknown error of ::accept " << savedErrno;
                break;
        }
    }
    return connfd;
}

auto Connect(int sockfd, const struct sockaddr* peer_addr)
    -> int
{
    return ::connect(sockfd, peer_addr, static_cast<socklen_t>(sizeof(struct sockaddr_storage)));
}

auto Read(int sockfd, void* buf, size_t count)
    -> ssize_t
{
    return ::read(sockfd, buf, count);
}

ssize_t Readv(int sockfd, const struct iovec* iov, int iovcnt)
{
    return ::readv(sockfd, iov, iovcnt);
}

ssize_t Write(int sockfd, const void* buf, size_t count)
{
    return ::write(sockfd, buf, count);
}

void close(int sockfd)
{
    if (::close(sockfd) < 0)
    {
        // LOG_SYSERR << "close";
    }
}

void ShutdownWrite(int sockfd)
{
    if (::shutdown(sockfd, SHUT_WR) < 0)
    {
        // LOG_SYSERR << "shutdownWrite";
    }
}

void toIpPortRepr(char* buf, size_t size, const struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET6)
    {
        buf[0] = '[';
        toIp(buf + 1, size - 1, addr);
        auto end          = ::strlen(buf);
        const auto* addr6 = sockaddrCast<sockaddr_in6>(addr);
        auto port         = networkToHost16(addr6->sin6_port);
        assert(size > end);
        snprintf(buf + end, size - end, "]:%u", port);
        return;
    }
    toIp(buf, size, addr);
    auto end          = ::strlen(buf);
    const auto* addr4 = sockaddrCast<sockaddr_in>(addr);
    auto port         = networkToHost16(addr4->sin_port);
    assert(size > end);
    snprintf(buf + end, size - end, ":%u", port);
}

void toIp(char* buf, size_t size, const struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET)
    {
        assert(size >= INET_ADDRSTRLEN);
        const auto* addr4 = sockaddrCast<sockaddr_in>(addr);
        ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(size));
    }
    else if (addr->sa_family == AF_INET6)
    {
        assert(size >= INET6_ADDRSTRLEN);
        const auto* addr6 = sockaddrCast<sockaddr_in6>(addr);
        ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, static_cast<socklen_t>(size));
    }
}

void FromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port   = HostToNetwork16(port);
    if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
    {
        // LOG_SYSERR << "fromIpPort";
    }
}

void FromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr)
{
    addr->sin6_family = AF_INET6;
    addr->sin6_port   = HostToNetwork16(port);
    if (::inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0)
    {
        // LOG_SYSERR << "fromIpPort";
    }
}

auto getSocketError(int sockfd) -> int
{
    int optval;
    auto optlen = static_cast<socklen_t>(sizeof optval);

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        return errno;
    }

    return optval;
}

auto getLocalAddr(int sockfd)
    -> struct sockaddr_storage
{
    struct sockaddr_storage local_addr
    {
    };
    // memZero(&localaddr, sizeof localaddr);
    auto addrlen = static_cast<socklen_t>(sizeof local_addr);
    if (::getsockname(sockfd, sockaddrCast<sockaddr>(&local_addr), &addrlen) < 0)
    {
        // LOG_SYSERR << "getLocalAddr";
    }
    return local_addr;
}

auto
getPeerAddr(int sockfd)
    -> struct sockaddr_storage
{
    struct sockaddr_storage peeraddr
    {
    };
    auto addrlen = static_cast<socklen_t>(sizeof peeraddr);
    if (::getpeername(sockfd, sockaddrCast<sockaddr>(&peeraddr), &addrlen) < 0)
    {
        // LOG_SYSERR << "getPeerAddr";
    }
    return peeraddr;
}

auto
isSelfConnect(int sockfd) -> bool
{
    auto localaddr = getLocalAddr(sockfd);
    auto peeraddr  = getPeerAddr(sockfd);
    if (localaddr.ss_family == AF_INET)
    {
        const auto* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
        const auto* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
        return laddr4->sin_port == raddr4->sin_port
               && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
    }
    if (localaddr.ss_family == AF_INET6)
    {

        const auto* laddr6 = reinterpret_cast<struct sockaddr_in6*>(&localaddr);
        const auto* raddr6 = reinterpret_cast<struct sockaddr_in6*>(&peeraddr);
        return laddr6->sin6_port == raddr6->sin6_port
               && memcmp(&laddr6->sin6_addr, &raddr6->sin6_addr, sizeof laddr6->sin6_addr) == 0;
    }

    return false;
}

} // namespace Sock