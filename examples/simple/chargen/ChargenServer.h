#include "net/Callbacks.h"
#include "net/InetAddress.h"
#include "net/TcpServer.h"
#include "net/Timestamp.h"

class ChargenServer{
private:
    std::shared_ptr<TcpServer> server_;
    std::string msg_;
    uint64_t transferred_;
    Timestamp start_time_;

public:
    ChargenServer(EventLoop* loop,
                  const InetAddress& listenAddr,
                  bool print = false);

    void start();

private:
    void onConnection_(const TcpConnectionPtr& conn);
    void onMessage_(const TcpConnectionPtr& conn,
                    Buffer& buf,
                    Timestamp ts);

    void onWriteComplete_(const TcpConnectionPtr& conn);
    void printThroughout_();

};