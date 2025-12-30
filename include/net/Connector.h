// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CONNECTOR_H
#define MUDUO_NET_CONNECTOR_H

#include "Timer.h"
#include "InetAddress.h"
#include "net/TcpClient.h"

#include <functional>
#include <memory>

class Channel;
class EventLoop;

/**
 * @tag inner implementation class, user invisible
 * @brief only reponsible for socket connection establishment, not for tcpconnection
 */
class Connector {
friend Channel;
public:
    using NewConnectionCallback = std::function<void(int sockFd)>;

private:
    enum States { Disconnected = 0,
                  Connecting, // 已开始连接，在这里Socket connect是非阻塞的
                  Connected };// tcpconnection
    static inline constexpr int c_max_retry_delay_ms  = 30 * 1000;
    static inline constexpr int c_init_retry_delay_ms = 500;

    EventLoop* const loop_;
    InetAddress peer_addr_;
    States state_;            // FIXME: use atomic variable ? no! we keep all the state_ transfer in loop thread_
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback new_connection_callback_;
    int sock_fd_; // 连接成功时，打开的sockfd
    int retry_delay_ms_;
    Timer::Id retry_timer_id_;

    void setState_(States s) { state_ = s; }
    /**
     * @brief 在 loop 线程里真正发起 connect, best effort
     */
    void startInLoop_();
    /**
     * @brief 取消当前连接流程, i.e. 取消建立连接
     */
    void stopInLoop_();

    /**
     * @brief 创建 socket + 发起非阻塞 connect
     */
    void connectPeer_();
    /**
     * @brief socket 连接建立后，初始化 tcpconnection
     */
    void initConnectChannel_(int sockfd);
    void handleWrite_();
    void handleError_();

    /**
     * @brief 关闭旧的sockfd并且重试
     */
    void retry_(bool delayResetChannel);

    auto removeAndResetChannel_(bool delayReset)->void;

    void resetSockFd_();

    void cancelReryTimer_();

    void restartInLoop_();

    void resetConnector_(bool delayResetChannel);
public:
    Connector(const Connector&)                    = delete;
    Connector(Connector&&)                         = delete;
    auto operator=(const Connector&) -> Connector& = delete;
    auto operator=(Connector&&) -> Connector&      = delete;

    Connector(EventLoop* loop, const InetAddress& peerAddr);
    /**
     * @brief must call stop before ~Connector
     */
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        new_connection_callback_ = cb;
    }
    /**
     * @brief 开始（或允许）连接流程
     * can be called in any thread
     */
    void start();

    void restart();

    /**
     * @brief 请求取消当前连接流程
     */
    void stop(); // can be called in any thread

    [[nodiscard]] auto getServerAddress() const
        -> const InetAddress& { return peer_addr_; }
};

#endif // MUDUO_NET_CONNECTOR_H