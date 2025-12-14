#include "DaytimeServer.h"

DaytimeServer::DaytimeServer(EventLoop* loop,
                             const InetAddress& listenAddr)
  : server_(loop, listenAddr, "DaytimeServer")
{
    server_.setConnectionEstablishedCallback([this](const auto& conn) {
        this->onConnection(conn);
    });

    server_.setConnectionCloseCallback([this](const auto& conn) {
        this->onConnection(conn);
    });
    server_.setMessageCallback([this](const auto& conn, auto& buf, auto timestamp) {
        this->onMessage(conn, buf, timestamp);
    });
}

void DaytimeServer::start()
{
  server_.start();
}

void DaytimeServer::onConnection(const TcpConnectionPtr& conn)
{
    std::println("tcpconn:{}", conn->getName());
    if (conn->isConnected())
    {
        std::println("DaytimeServer - {}->{} is UP", conn->getPeerAddress().toIpPortRepr(), conn->getLocalAddress().toIpPortRepr());
        conn->send(Timestamp::Now().toFormattedString() + "\n");
        conn->shutdown();
    }
    else if (conn->isDisconnected())
    {
        std::println("DaytimeServer - {}->{} is DOWN", conn->getPeerAddress().toIpPortRepr(), conn->getLocalAddress().toIpPortRepr());
    }
}

void DaytimeServer::onMessage(const TcpConnectionPtr& conn,
                              Buffer& buf,
                              Timestamp time)
{
  auto msg = buf.readAllAsString();
  std::println("{}:{} bytes discards at {}", conn->getName(), msg.size(), time.toString());
}
