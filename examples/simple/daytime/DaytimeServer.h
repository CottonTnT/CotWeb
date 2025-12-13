#ifndef MUDUO_EXAMPLES_SIMPLE_DAYTIME_DAYTIME_H
#define MUDUO_EXAMPLES_SIMPLE_DAYTIME_DAYTIME_H

#include "net/TcpServer.h"

// RFC 867
class DaytimeServer
{
 public:
  DaytimeServer(EventLoop* loop,
                const InetAddress& listenAddr);

  void start();

 private:
  void onConnection(const TcpConnectionPtr& conn);


  void onMessage(const TcpConnectionPtr& conn,
                 Buffer& buf,
                 Timestamp time);

  TcpServer server_;
};

#endif  // MUDUO_EXAMPLES_SIMPLE_DAYTIME_DAYTIME_H