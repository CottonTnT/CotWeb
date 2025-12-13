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

#include "InetAddress.h"

#include <atomic>
#include <functional>
#include <memory>

class Channel;
class EventLoop;

/**
 * @tag inner implementation class, user invisible
 * @brief 
 */
class Connector : public std::enable_shared_from_this<Connector> {
public:
    using NewConnectionCallback = std::function<void(int)>;

    Connector(const Connector&)                    = delete;
    Connector(Connector&&)                         = delete;
    auto operator=(const Connector&) -> Connector& = delete;
    auto operator=(Connector&&) -> Connector&      = delete;

    Connector(EventLoop* loop, const InetAddress& serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        new_connection_callback_ = cb;
    }

    void start();   // can be called in any thread
    void restart(); // must be called in loop thread
    void stop();    // can be called in any thread

    auto getServerAddress() const
        -> const InetAddress& { return server_addr_; }

private:
    enum States { kDisconnected,
                  kConnecting,
                  kConnected };
    static inline constexpr int c_max_retry_delay_ms  = 30 * 1000;
    static inline constexpr int c_init_retry_delay_ms = 500;

    void setState_(States s) { state_ = s; }
    void startInLoop_();
    void stopInLoop_();
    void connect_();
    void connecting_(int sockfd);
    void HandleWrite_();
    void HandleError_();
    void retry_(int sockfd);
    auto removeAndResetChannel_() -> int;
    void resetChannel_();

    EventLoop* loop_;
    InetAddress server_addr_;
    std::atomic<bool> start_connect_; // atomic, 是否开始连接
    States state_; // FIXME: use atomic variable
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback new_connection_callback_;
    int retry_delay_ms_;
};

#endif // MUDUO_NET_CONNECTOR_H