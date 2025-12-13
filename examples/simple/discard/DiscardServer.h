#pragma once
#include "net/Callbacks.h"
#include "net/TcpServer.h"

// RFC 863
class DiscardServer
{
 public:
  DiscardServer(EventLoop* loop,
                const InetAddress& listenAddr);

  void start();

 private:
  void onConnectionEstablished(const TcpConnectionPtr& conn);
  void onConnectionClose(const TcpConnectionPtr& conn);

  void onMessage(const TcpConnectionPtr& conn,
                 Buffer& buf,
                 Timestamp time);

  TcpServer server_;
};