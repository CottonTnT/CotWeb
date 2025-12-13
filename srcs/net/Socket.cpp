#include <expected>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <cstring>

#include "net/Socket.h"
// #include "Logger.h"
#include "net/InetAddress.h"
#include "net/Socketsops.h"


Socket::~Socket()
{
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr) const
{
    auto sock_addr_size = localaddr.getFamily() == AF_INET
                              ? sizeof(sockaddr_in)
                              : sizeof(sockaddr_in6);
    if (0 != ::bind(sockfd_, localaddr.getSockAddr(), sock_addr_size))
    {
        // LOG_FATAL("bind sockfd:%d fail\n", sockfd_);
    }
}

void Socket::listen() const
{
    if (0 != ::listen(sockfd_, 1024))
    {
        // LOG_FATAL("listen sockfd:%d fail\n", sockfd_);
    }
}

auto Socket::Accept(InetAddress* peeraddr) const
    -> int
{
    auto addr = sockaddr_storage{};
    auto len = static_cast<socklen_t>(sizeof(addr));
    auto connfd = ::accept4(sockfd_, Sock::sockaddrCast<sockaddr>(&addr), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (connfd >= 0)
    {
        peeraddr->SetSockAddr(Sock::sockaddrCast<sockaddr>(&addr));
    }
    return connfd;
}

auto Socket::accept(InetAddress& peeraddr) const
    -> std::expected<int, std::error_code>
{
    auto addr = sockaddr_storage{};
    auto len = static_cast<socklen_t>(sizeof(addr));
    auto connfd = ::accept4(sockfd_, Sock::sockaddrCast<sockaddr>(&addr), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (connfd >= 0)
    {
        peeraddr.SetSockAddr(Sock::sockaddrCast<sockaddr>(&addr));
        return connfd;
    }
    return std::unexpected<std::error_code>{std::make_error_code(std::errc{errno})};
}

void Socket::shutdownWrite() const
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        //todo:log
        // LOG_ERROR("shutdownWrite error");
    }
}

void Socket::SetTcpNoDelay(bool on) const
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)); // TCP_NODELAY包含头文件 <netinet/tcp.h>
}
void Socket::setReuseAddr(bool on) const
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); // TCP_NODELAY包含头文件 <netinet/tcp.h>
}
void Socket::setReusePort(bool on) const
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)); // TCP_NODELAY包含头文件 <netinet/tcp.h>
}
void Socket::setKeepAlive(bool on) const
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)); // TCP_NODELAY包含头文件 <netinet/tcp.h>
}

auto Socket::getTcpInfo(struct tcp_info* tcpi) const
    -> bool
{
  socklen_t len = sizeof(*tcpi);
  std::memset(tcpi, 0 , len);
  return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

auto Socket::getTcpInfoString(char* buf, int len) const
    -> bool
{
  struct tcp_info tcpi;
  auto ok = getTcpInfo(&tcpi);
  if (ok)
  {
    snprintf(buf, len, "unrecovered=%u "
             "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
             "lost=%u retrans=%u rtt=%u rttvar=%u "
             "sshthresh=%u cwnd=%u total_retrans=%u",
             tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
             tcpi.tcpi_rto,          // Retransmit timeout in usec
             tcpi.tcpi_ato,          // Predicted tick of soft clock in usec
             tcpi.tcpi_snd_mss,
             tcpi.tcpi_rcv_mss,
             tcpi.tcpi_lost,         // Lost packets
             tcpi.tcpi_retrans,      // Retransmitted packets out
             tcpi.tcpi_rtt,          // Smoothed round trip time in usec
             tcpi.tcpi_rttvar,       // Medium deviation
             tcpi.tcpi_snd_ssthresh,
             tcpi.tcpi_snd_cwnd,
             tcpi.tcpi_total_retrans);  // Total retransmits for entire connection
  }
  return ok;
}