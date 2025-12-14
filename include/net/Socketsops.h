// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_SOCKETSOPS_H
#define MUDUO_NET_SOCKETSOPS_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <type_traits>

namespace Sock {

///
/// Creates a non-blocking socket file descriptor,
/// abort if any error.
auto createNonblockingOrDie(sa_family_t family)
    -> int;

auto connect(int sockfd, const struct sockaddr& peer_addr)
    -> int;
void BindOrDie(int sockfd, const struct sockaddr* addr);

void ListenOrDie(int sockfd);
auto Accept(int sockfd, struct sockaddr_in6* addr)
    -> int;
auto Read(int sockfd, void* buf, size_t count)
    -> ssize_t;
auto Readv(int sockfd, const struct iovec* iov, int iovcnt)
    -> ssize_t;
auto Write(int sockfd, const void* buf, size_t count)
    -> ssize_t;

void close(int sockfd);

void ShutdownWrite(int sockfd);

void toIpPortRepr(char* buf, size_t size, const struct sockaddr* addr);
void toIp(char* buf, size_t size, const struct sockaddr* addr);

void FromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr);
void FromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr);

auto getSocketError(int sockfd) -> int;

template <typename T>
concept IsIpv4SocketAddr = std::is_same_v<std::decay_t<T>, struct sockaddr_in>;

template <typename T>
concept IsIpv6SocketAddr = std::is_same_v<std::decay_t<T>, struct sockaddr_in6>;

template <typename T>
concept IsSockStorage = std::is_same_v<std::decay_t<T>, struct sockaddr_storage>;

template <typename T>
concept IsGeneralSocketAddr = std::is_same_v<std::decay_t<T>, struct sockaddr>;

template <typename T>
concept IsSocketAddr = IsIpv4SocketAddr<T>
                       or IsIpv6SocketAddr<T>
                       or IsGeneralSocketAddr<T>
                       or IsSockStorage<T>;

/**
 * @brief 支持任意两个socket地址类型之间的转换,
 * @attention caller must ensure the correctness of the conversion
 */
template <IsSocketAddr To, IsSocketAddr From>
auto sockaddrCast(From* from_addr)
    -> decltype(auto)
{
    if constexpr (std::is_const_v<From>)
        return reinterpret_cast<const To*>(from_addr);
    else
        return reinterpret_cast<To*>(from_addr);
}

auto getLocalAddr(int sockfd) -> struct sockaddr_storage;

auto getPeerAddr(int sockfd) -> struct sockaddr_storage;
auto isSelfConnect(int sockfd) -> bool;

} // namespace Sock

#endif // MUDUO_NET_SOCKETSOPS_H