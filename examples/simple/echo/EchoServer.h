#pragma once

#include "net/Callbacks.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpServer.h"
#include "net/Timestamp.h"

class EchoServer {
private:
    std::shared_ptr<TcpServer> server_;

public:
    EchoServer(EventLoop* loop,
               const InetAddress& listenAddr);
    void start();

private:
    void onConnection_(const TcpConnectionPtr& conn);

    void onMessage_(const TcpConnectionPtr& conn,
                   Buffer& buf,
                   Timestamp time);
};
