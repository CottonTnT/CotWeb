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

#include <atomic>
#include <functional>
#include <memory>

class Channel;
class EventLoop;

/**
 * @tag inner implementation class, user invisible
 * @brief only reponsible for socket connection establishment, not for tcpconnection
 */
class Connector {
public:
    using NewConnectionCallback = std::function<void(int sockFd)>;

private:
    enum States { Disconnected,
                  Connecting,
                  Connected };
    static inline constexpr int c_max_retry_delay_ms  = 30 * 1000;
    static inline constexpr int c_init_retry_delay_ms = 500;

    EventLoop* const loop_;
    InetAddress peer_addr_;
    std::atomic<bool> in_connecting_; // atomic, 表示 Connector 是否保持连接, 即是否在尝试连接或重试或已连接
    std::atomic<States> state_;       // FIXME: use atomic variable
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback new_connection_callback_;
    int retry_delay_ms_;
    Timer::Id retry_timer_id_;

    void setState_(States s) { state_ = s; }
    void startInLoop_();
    void stopInLoop_();
    void connectPeer_();
    /**
     * @brief 连接建立后初始化工作
     */
    void postSocketConnected_(int sockfd);
    void channelWriteCB_();
    void channelErrorCB_();
    /**
     * @brief 关闭旧的sockfd并且重试
     */
    void retry_(int sockfd);

    void closeNow_(int sockfd);
    auto removeAndResetChannel_() -> int;
    void resetChannel_();

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
     * @brief 用户开始连接或重试
     * can be called in any thread
     */
    void start();

    /**
     * @brief must be called in loop thread
     */
    void restart();

    void stop(); // can be called in any thread

    [[nodiscard]] auto getServerAddress() const
        -> const InetAddress& { return peer_addr_; }
};

#endif // MUDUO_NET_CONNECTOR_H