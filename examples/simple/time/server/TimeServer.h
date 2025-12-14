#pragma once
#include "net/TcpConnection.h"
#include "net/TcpServer.h"
#include <memory>

// RFC 868
class TimeServer {

private:
    std::shared_ptr<TcpServer> server_;
    void onConnection(const TcpConnectionPtr& conn);

    void onMessage(const TcpConnectionPtr& conn,
                   Buffer& buf,
                   Timestamp time);

public:
    TimeServer(EventLoop* loop,
               const InetAddress& listenAddr);

    void start();
};