#include "TimeServer.h"
#include "logger/LogLevel.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"

#include "net/Endian.h"
#include "net/TcpServer.h"

static auto log = GET_ROOT_LOGGER();
TimeServer::TimeServer(EventLoop* loop,
                       const InetAddress& listenAddr)
    : server_ {new TcpServer {loop, listenAddr, "TimeServer"}}
{
    server_->setConnectionEstablishedCallback([this](const auto& conn) {
        this->onConnection(conn);
    });
    server_->setConnectionCloseCallback([this](const auto& conn) {
        this->onConnection(conn);
    });
    server_->setMessageCallback([this](const auto& conn, auto& buf, auto ts) {
        this->onMessage(conn, buf, ts);
    });
}

void TimeServer::start()
{
    server_->start();
}

void TimeServer::onConnection(const TcpConnectionPtr& conn)
{
    LOG_DEBUG_FMT(log, "TimeServer - {} ->  is  {}", conn->getPeerAddress().toIpPortRepr(), conn->isConnected() ? "UP" : "DOWN");
    if (conn->isConnected())
    {
        time_t now   = ::time(NULL);

        Timestamp ts(static_cast<uint64_t>(now) * Timestamp::c_micro_seconds_per_second);
        LOG_INFO_FMT(log, "Server time = {}, {}", now, ts.toFormattedString());
        int32_t be32 = Sock::hostToNetwork32(static_cast<int32_t>(now));
        conn->send(be32);
        conn->shutdown();
    }
}

void TimeServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer& buf,
                           Timestamp time)
{
    auto msg = std::string {buf.readAllAsString()};
    LOG_INFO_FMT(log, "{} discards {} bytes received at {}", conn->getName(), msg.size(), time.toString());
}
