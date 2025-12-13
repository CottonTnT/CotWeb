#include "DiscardServer.h"
#include "net/Timestamp.h"
#include <print>

DiscardServer::DiscardServer(EventLoop* loop,
                             const InetAddress& listenAddr)
    : server_(loop, listenAddr, "DiscardServer")
{
    server_.setConnectionEstablishedCallback([this](const auto& tcpconn) {
        this->onConnectionEstablished(tcpconn);
    });
    server_.setConnectionCloseCallback([this](const auto& tcpconn) {
        this->onConnectionClose(tcpconn);
    });
    server_.setMessageCallback(
        [this](auto& tcpconn, auto& buf, auto timestamp) { onMessage(tcpconn, buf, timestamp); });
    server_.setThreadNum(2);
}

void DiscardServer::start()
{
    server_.start();
}

void DiscardServer::onConnectionEstablished(const TcpConnectionPtr& conn)
{
    std::println("tcpconn:{}", conn->getName());
    std::println("DiscardServer - {}->{} is UP", conn->getPeerAddress().toIpPortRepr(), conn->getLocalAddress().toIpPortRepr());
}

void DiscardServer::onConnectionClose(const TcpConnectionPtr& conn)
{
    std::println("tcpconn:{}", conn->getName());
    std::println("DiscardServer - {}->{} is DOWN", conn->getPeerAddress().toIpPortRepr(), conn->getLocalAddress().toIpPortRepr());
}

void DiscardServer::onMessage(const TcpConnectionPtr& conn,
                              Buffer& buf,
                              Timestamp time)
{
    auto msg = buf.readAllAsString();
    std::println("msg:{}", msg);
    std::println("{} discards {} bytes received at {}", conn->getName(), msg.size(), time.toString());
}
