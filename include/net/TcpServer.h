#pragma once

/**
 * 用户使用muduo编写服务器程序
 **/

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "EventLoop.h"
#include "Acceptor.h"
#include "Callbacks.h"
#include "EventLoopThreadpool.h"
#include "InetAddress.h"
#include "TcpConnection.h"

/**
 * @brief manager of tcpconnection, dont
 */
class TcpServer {
public:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option {
        kNoReusePort,
        kReusePort,
    };

private:
    /**
     * @brief tcpconnection name-> tcpconnection
     */

    EventLoop* base_loop_; // baseloop 用户自定义的loop

    const std::string ipport_repr_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; // 运行在 mainloop 任务就是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadpool_; // one loop per thread

    ConnectionCallback conn_established_callback_; // 有新连接建立完成时的回调时的回调
    ConnectionCallback conn_close_callback_;       // 有连接关闭时的回调

    MessageCallback msg_callback_;                  // 有读写事件发生时的回调
    WriteCompleteCallback write_complete_callback_; // 消息发送完成后的回调

    ThreadInitCallback thread_init_callback_; // loop线程初始化的回调

    std::atomic_int started_;

    int next_conn_id_;
    ConnectionMap connections_; // 保存所有的连接
public:
    TcpServer(EventLoop* loop,
              const InetAddress& listenAddr,
              std::string nameArg,
              Option option = kNoReusePort);
    ~TcpServer();

    TcpServer(const TcpServer&)                    = delete;
    auto operator=(const TcpServer&) -> TcpServer& = delete;
    TcpServer(TcpServer&&)                         = delete;
    auto operator=(TcpServer&&) -> TcpServer&      = delete;

    void SetThreadInitCallback(const ThreadInitCallback& cb) { thread_init_callback_ = cb; }

    // /**
    //  * @brief user-level callback for connection established and connection close
    //  * @not thread safe
    //  */
    // void SetConnectionCallback(const ConnectionCallback &cb) { conn_callback_ = cb; }

    /**
     * @brief user-level callback for connection established
     * @not thread safe
     */
    void setConnectionEstablishedCallback(const ConnectionCallback& cb) { conn_established_callback_ = cb; }

    /**
     * @brief user-level callback for connection established
     * @not thread safe
     */
    void setConnectionCloseCallback(const ConnectionCallback& cb) { conn_close_callback_ = cb; }

    /**
     * @brief user-level callback  for message arrival
     * @not thread safe
     */
    void setMessageCallback(const MessageCallback& cb) { msg_callback_ = cb; }

    /**
     * @brief user-level callback for message write complete
     * @not thread safe
     */
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { write_complete_callback_ = cb; }

    /**
     * @brief Set the number of threads for handling input.
     * @attention 1. Always accepts new connection in loop's thread.
     * 2. Must be called before start() is called.
     - 0 means all I/O in loop's thread, no thread will created.
       this is the default value.
     - 1 means all I/O in another thread.
     - N means a thread pool with N threads, new connections
       are assigned on a round-robin basis.
     * @todo: limit the connection number
     */
    void setThreadNum(int numThreads);

    /**
     * @brief Starts the server if it's not listening.It's harmless to call it multiple times.
     * @thread safe
     */
    void start();

private:
    /**
     * @brief
     * @details Not thread safe, but in loop
     */
    void initNewConnInOwnerThread_(int sockfd, const InetAddress& peerAddr);

    /**
     * @details Thread safe, 通过此函数会将实际的移除操作（从 connections_ map 中删除）提交到 TcpServer 的主 EventLoop 中执行（removeConnectionInLoop），以保证线程安全。
     */
    void removeConnection_(const TcpConnectionPtr& conn);

    /**
     * @brief
     * @details Not thread safe, but in loop
     */
    void removeConnectionInOwnerThread_(const TcpConnectionPtr& conn);
};