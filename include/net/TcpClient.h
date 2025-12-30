#pragma once

#include "net/Callbacks.h"
#include "TcpConnection.h"
#include "net/InetAddress.h"
#include <memory>

class Connector;
using ConnectorPtr = std::unique_ptr<Connector>;

/**
 * @brief only thread-safe when use it in the same thread with loop
 */
class TcpClient : public std::enable_shared_from_this<TcpClient>{

private:

    EventLoop* const loop_;
    ConnectorPtr connector_; // avoid revealing Connector
    const std::string name_;
    ConnectionCallback conn_established_callback_;
    ConnectionCallback conn_close_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    bool keep_retry_;       // atomic
    bool willing_to_connect_;// 表示用户是否希望保持与服务端连接
    // always in loop thread
    int next_conn_id_;
    TcpConnectionPtr connection_;
    const InetAddress server_addr_;


    /// Not thread safe, but in loop
    void initTcpConnection_(int sockfd);
    /// Not thread safe, but in loop
    void removeConn_(const TcpConnectionPtr& conn);

    void removeConnInLoop_(const TcpConnectionPtr& conn);
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

    /**
     * @brief not TS
     */
    void connect();
    /**
     * @brief not TS
     */
    void disconnect();
    /**
     * @brief note TS
     */
    void stop();


    auto getLoop() const -> const EventLoop* { return loop_; }
    auto retry() const -> bool { return keep_retry_; }
    void enableRetry() { keep_retry_ = true; }

    auto getName() const
        -> const std::string&
    {
        return name_;
    }

    /// Set connection callback.
    /// Not thread safe.
    void setConnetionCallback(ConnectionCallback cb)
    {
        conn_established_callback_ = std::move(cb);
    }

    void setConnectionCloseCallback(ConnectionCallback cb)
    {
        conn_close_callback_ = std::move(cb);
    }
    /// Set message callback.
    /// Not thread s: public std::enable_shared_from_this<Connector>afe.
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
