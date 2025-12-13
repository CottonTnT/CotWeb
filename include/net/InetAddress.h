#pragma once

#include "net/Socketsops.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <variant>

// 封装socket地址类型
class InetAddress {
public:
    /**
     * @brief Constructs an endpoint with given port number.Mostly used in TcpServer listening.
     */
    explicit InetAddress(uint16_t port = 0, bool loopback_only = false, bool ipv6 = false);

    /// Constructs an endpoint with given ip and port.
    /// @c ip should be "1.2.3.4"
    InetAddress(std::string ip, uint16_t port, bool ipv6 = false);

    /// Constructs an endpoint with given struct @c sockaddr_in
    /// Mostly used when accepting new connections
    explicit InetAddress(const sockaddr_in& addr)
        : addr_(addr)
    {
    }

    explicit InetAddress(const struct sockaddr_in6& addr)
        : addr_(addr)
    {
    }
    explicit InetAddress(const sockaddr_storage& other)
    {
        if (other.ss_family == AF_INET)
        {
            addr_ = *Sock::sockaddrCast<sockaddr_in>(&other);
        }
        else if (other.ss_family == AF_INET6)
        {
            addr_ = *Sock::sockaddrCast<sockaddr_in6>(&other);
        }
        else
        {
            //todo
        }
    }

    explicit InetAddress(const struct sockaddr* addr)
    {
        if (addr->sa_family == AF_INET)
        {
            addr_ = *Sock::sockaddrCast<sockaddr_in>(addr);
            return;
        }
        if (addr->sa_family == AF_INET6)
        {
            addr_ = *Sock::sockaddrCast<sockaddr_in6>(addr);
            return;
        }
        // todo: error_handling
    }

    [[nodiscard]] auto getFamily() const
        -> sa_family_t
    {
        if (addr_.index() == 0)
            return std::get<sockaddr_in>(addr_).sin_family;
        return std::get<sockaddr_in6>(addr_).sin6_family;
    }
    [[nodiscard]] auto toIpRepr() const
        -> std::string;

    [[nodiscard]] auto toIpPortRepr() const
        -> std::string;
    [[nodiscard]] auto GetPort() const
        -> uint16_t;

    [[nodiscard]] auto getSockAddr() const
        -> const struct sockaddr*;

    void SetSockAddr(struct sockaddr* addr)
    {
        if (addr->sa_family == AF_INET)
        {
            std::get<sockaddr_in>(addr_) = *Sock::sockaddrCast<sockaddr_in>(addr);
        }
        else if (addr->sa_family == AF_INET6)
        {
            std::get<sockaddr_in6>(addr_) = *Sock::sockaddrCast<sockaddr_in6>(addr);
        }
    }

    [[nodiscard]] auto Ipv4NetEndian() const -> uint32_t;

    [[nodiscard]] auto PortNetEndian() const -> uint16_t
    {
        if (addr_.index() == 0)
            return std::get<sockaddr_in>(addr_).sin_port;
        return std::get<sockaddr_in6>(addr_).sin6_port;
    }

    // resolve hostname to IP address, not changing port or sin_family
    // return true on success.
    // thread safe
    static auto Resolve(std::string hostname, InetAddress* result) -> bool;
    // static std::vector<InetAddress> resolveAll(const char* hostname, uint16_t port = 0);

    // set IPv6 ScopeID
    void SetScopeId(uint32_t scope_id);

    static auto GetLocalInetAddress(int sockfd) -> InetAddress;

    static auto GetPeerInetAddress(int sockfd) -> InetAddress;

private:
    std::variant<sockaddr_in, sockaddr_in6> addr_;
};
