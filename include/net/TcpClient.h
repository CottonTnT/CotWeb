// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCLIENT_H
#define MUDUO_NET_TCPCLIENT_H

#include "net/Callbacks.h"
#include "TcpConnection.h"
#include <atomic>

class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient {

private:
    /// Not thread safe, but in loop
    void initNewConnection_(int sockfd);
    /// Not thread safe, but in loop
    void RemoveConnection_(const TcpConnectionPtr& conn);

    /**
     * @attention the client dont not own the loop unlike the server
     */
    EventLoop* loop_;
    ConnectorPtr connector_; // avoid revealing Connector
    const std::string name_;
    ConnectionCallback conn_established_callback_;
    ConnectionCallback conn_close_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    std::atomic<bool> retry_;   // atomic
    std::atomic<bool> not_connect_; // atomic
    // always in loop thread
    int next_conn_id_;
    mutable std::mutex mutex_;
    TcpConnectionPtr connection_;
public:
    TcpClient(const TcpClient&)                    = delete;
    TcpClient(TcpClient&&)                         = delete;
    auto operator=(const TcpClient&) -> TcpClient& = delete;
    auto operator=(TcpClient&&) -> TcpClient&      = delete;

    // TcpClient(EventLoop* loop);
    // TcpClient(EventLoop* loop, const string& host, uint16_t port);
    TcpClient(EventLoop* loop,
              const InetAddress& serverAddr,
              std::string nameArg);
    ~TcpClient(); // force out-line dtor, for std::unique_ptr members.

    void connect();
    void Disconnect();
    void Stop();

    auto GetConnection() const
        -> TcpConnectionPtr
    {
        auto _ = std::lock_guard<std::mutex> {mutex_};
        return connection_;
    }

    auto GetLoop() const -> EventLoop* { return loop_; }
    auto Retry() const -> bool { return retry_; }
    void EnableRetry() { retry_ = true; }

    auto GetName() const
        -> const std::string&
    {
        return name_;
    }

    /// Set connection callback.
    /// Not thread safe.
    void SetConnectionEstablishedCallback(ConnectionCallback cb)
    {
        conn_established_callback_ = std::move(cb);
    }

    void SetConnectionCloseCallback(ConnectionCallback cb)
    {
        conn_close_callback_ = std::move(cb);
    }
    /// Set message callback.
    /// Not thread safe.
    void SetMessageCallback(MessageCallback cb)
    {
        message_callback_ = std::move(cb);
    }

    /// Set write complete callback.
    /// Not thread safe.
    void SetWriteCompleteCallback(WriteCompleteCallback cb)
    {
        write_complete_callback_ = std::move(cb);
    }

};

#endif // MUDUO_NET_TCPCLIENT_H