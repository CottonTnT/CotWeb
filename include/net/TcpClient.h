#pragma once

#include "net/Callbacks.h"
#include "TcpConnection.h"
#include <atomic>
#include <memory>

class Connector;
using ConnectorPtr = std::unique_ptr<Connector>;

/**
 * @brief only thread-safe when use it in the same thread with loop
 */
class TcpClient : std::enable_shared_from_this<TcpClient>{

private:
    /// Not thread safe, but in loop
    void initTcpConnection_(int sockfd);
    /// Not thread safe, but in loop
    void removeConnection_(const TcpConnectionPtr& conn);

    EventLoop* const loop_;
    ConnectorPtr connector_; // avoid revealing Connector
    const std::string name_;
    ConnectionCallback connection_callback_;
    ConnectionCallback conn_close_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    std::atomic<bool> retry_;       // atomic
    std::atomic<bool> already_start_connect_;
    // std::atomic<bool> keep_connection_; // atomic, 客户端是否期望与服务器连接
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
    void disconnect();
    void stop();

    auto getConnection() const
        -> TcpConnectionPtr
    {
        auto _ = std::lock_guard<std::mutex> {mutex_};
        return connection_;
    }

    auto getLoop() const -> const EventLoop* { return loop_; }
    auto retry() const -> bool { return retry_; }
    void enableRetry() { retry_ = true; }

    auto getName() const
        -> const std::string&
    {
        return name_;
    }

    /// Set connection callback.
    /// Not thread safe.
    void setConnetionCallback(ConnectionCallback cb)
    {
        connection_callback_ = std::move(cb);
    }

    void setConnectionCloseCallback(ConnectionCallback cb)
    {
        conn_close_callback_ = std::move(cb);
    }
    /// Set message callback.
    /// Not thread safe.
    void setMessageCallback(MessageCallback cb)
    {
        message_callback_ = std::move(cb);
    }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(WriteCompleteCallback cb)
    {
        write_complete_callback_ = std::move(cb);
    }
};
