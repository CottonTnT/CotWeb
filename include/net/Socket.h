#pragma once
#include <expected>
#include <system_error>
class InetAddress;
/**
 * @tag inner implementation class for socket, user invisible
 * @brief  封装socket fd
 */
class Socket 
{
public:
    explicit Socket(int sockfd)
        : sockfd_{ sockfd }
    {
    }
    Socket(const Socket &) = delete;
    auto operator=(const Socket &) -> Socket & = delete;
    Socket(Socket &&) = delete;
    auto operator=(Socket &&) -> Socket & = delete;

    ~Socket();

    [[nodiscard]] auto GetFd() const
        -> int { return sockfd_; }
    void bindAddress(const InetAddress &localaddr) const; 
    void listen() const;
    /**
     * @brief if success, return a socket  fd and set the peeraddr, otherwise -1
     */
    auto Accept(InetAddress* peeraddr) const
        -> int;

    [[nodiscard]] auto accept(InetAddress& peeraddr) const
        -> std::expected<int, std::error_code>;
    void shutdownWrite() const;

    void SetTcpNoDelay(bool on) const;
    void setReuseAddr(bool on) const;
    void setReusePort(bool on) const;
    void setKeepAlive(bool on) const;
  // return true if success.
    auto getTcpInfo(struct tcp_info*) const
        -> bool;
    auto getTcpInfoString(char* buf, int len) const -> bool;

private:
    const int sockfd_;
};