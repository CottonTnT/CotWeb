#pragma once

#include <functional>

#include "net/Socket.h"
#include "net/Channel.h"

class EventLoop;
class InetAddress;
/**
 * @tag inner implementation class for tcpserver
 * @brief 监听新用户连接的类
 */
class Acceptor
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);

    ~Acceptor();

    Acceptor(const Acceptor &) = delete;
    auto operator=(const Acceptor &) -> Acceptor & = delete;
    Acceptor(Acceptor &&) = delete;
    auto operator=(Acceptor &&) -> Acceptor & = delete;

    /**
     * @brief user-level callback for new connection
     */
    void setNewConnectionCallback(const NewConnectionCallback &cb) { new_conn_callback_ = cb; }

    [[nodiscard]] auto listen() const
        -> bool { return listenning_; }
    void listenInOwnerThread();

    void cleanChannelInOnwerLoop_();

private:
    static void defautlNewConnectionCallback_(int sockfd)
    {
        ::close(sockfd); // 如果没有设置回调 就关闭这个连接
    }
    /**
     * @brief the callback for channel to deal with read event
     */
    void socketChannelReadCB_();

    EventLoop * const owner_loop_; // Acceptor用的就是用户定义的那个baseLoop 也称作mainLoop
    Socket accept_socket_;
    Channel listen_channel_;
    NewConnectionCallback new_conn_callback_;
    bool listenning_;
    int idle_fd_;
};