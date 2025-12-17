#include "EchoServer.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"
#include "net/Callbacks.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpServer.h"
#include "net/Timestamp.h"
#include <memory>

static auto log = GET_ROOT_LOGGER();
EchoServer::EchoServer(EventLoop* loop,
                       const InetAddress& listenAddr)
    : server_(new TcpServer{loop, listenAddr, "EchoServer"})
{
    server_->setConnectionEstablishedCallback([this](const auto& conn) {
        this->onConnection_(conn);
    });
    server_->setMessageCallback([this](const auto& conn,
                                      auto& buf,
                                      auto ts) {
        this->onMessage_(conn, buf, ts);
    });
}

void EchoServer::start()
{
    server_->start();
}

void EchoServer::onConnection_(const TcpConnectionPtr& conn)
{
    LOG_INFO_FMT(log, "EchoServer - {} -> {} is {}", conn->getPeerAddress().toIpPortRepr(), conn->getLocalAddress().toIpPortRepr(), conn->isConnected() ? "UP" : "DOWN");
}

void EchoServer::onMessage_(const TcpConnectionPtr& conn,
                            Buffer& buf,
                            Timestamp ts)
{
    auto msg = std::string {buf.readAllAsString()};
    LOG_INFO_FMT(log, "{} echo {} bytes, data received at {}", conn->getName(), msg.size(), ts.toString());
    conn->send(std::move(msg));
}
