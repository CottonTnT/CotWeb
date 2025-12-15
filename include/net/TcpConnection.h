#pragma once

#include <any>
#include <atomic>
#include <memory>
#include <string>

#include "net/Channel.h"
#include "net/Buffer.h"
#include "net/Callbacks.h"
#include "net/InetAddress.h"
#include "net/Timestamp.h"
#include "net/EventLoop.h"

class Channel;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection设置回调 => 设置到Channel => Poller => Channel回调
 **/

/**
 * @brief manager of socket fd and reponsible for the socket IO
 * @attention 1. TcpConnection负责关闭socket, 表示一次连接，是不可再生或复用的
 2. 没有发起连接的功能, 其构造函数是建立好连接的 socketfd, i.e initial state is kConnecting
 */
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
    friend class TcpServer;
    friend class TcpClient;

private:
    enum StateE {
        // 已经断开连接
        Disconnected,
        // 连接初始化中，设置注册事件等
        Connecting,
        // 已连接
        Connected,
        // 正在断开连接, 取消注册事件等
        Disconnecting
    };

    EventLoop* const owner_loop_; // one connection owned by one loop only
    const std::string name_;
    std::atomic<StateE> state_;
    bool reading_;

    // we dont expose those classes to client
    // Socket Channel 这里和Acceptor类似    Acceptor => mainloop    TcpConnection => subloop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> socket_channel_;

    const InetAddress local_addr_;
    const InetAddress peer_addr_;
    ConnectionCallback connection_callback_;        // 有新连接建立完成时的回调时的回调
    MessageCallback msg_callback_;                  // 有读写消息时的回调
    WriteCompleteCallback write_complete_callback_; // 消息发送完成以后的回调
    HighWaterMarkCallback high_watermark_callback_;

    CloseCallback close_callback_;
    size_t high_watermark_;

    // 数据缓冲区
    Buffer input_buf_;  // 接收数据的缓冲区
    Buffer output_buf_; // 发送数据的缓冲区 用户send向outputBuffer_发
    std::any context_;
    // FIXME: creationTime_, lastReceiveTime_
    //        bytesReceived_, bytesSent_
    void setState_(StateE state) { state_ = state; }

    void socketChannelReadCB_(Timestamp receiveTime);
    void socketChannelWriteCB_();

    void socketChannelErrorCB_();

    /**
     * @brief must be called in onwer loop
     */
    void sendInOwnerLoop_(const void* data, size_t len);
    void sendInOwnerLoop_(std::string_view message);

    void shutdownInOwnerLoop_();
    void forceCloseInOwnerLoop_();
    auto stateToString_() const
        -> std::string_view;
    void startReadInOwnerLoop_();
    void stopReadInOwnerLoop_();

    /* ======================== for tcpserver  ======================== */
    // called when TcpServer accepts a new connection
    // should be called only once
    void postConnectionCreate_();
    // called when TcpServer has removed me from its map
    // make sure the connection destruct in onwer loop thread
    void destructConnectionInOnwerLoop_(); // should be called only once
    void socketChannelCloseCB_();

public:
    /**
     * @attention User should not create this object.
     * @brief Constructs a TcpConnection with a connected sockfd
     */
    TcpConnection(EventLoop* loop,
                  std::string nameArg,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    auto operator=(const TcpConnection&)
        -> TcpConnection& = delete;

    TcpConnection(TcpConnection&&) = delete;
    auto operator=(TcpConnection&&)
        -> TcpConnection& = delete;

    auto getLoop() const
        -> EventLoop* { return owner_loop_; }

    auto getName() const
        -> const std::string& { return name_; }

    auto getLocalAddress() const
        -> const InetAddress& { return local_addr_; }

    auto getPeerAddress() const
        -> const InetAddress& { return peer_addr_; }

    auto isConnected() const
        -> bool { return state_ == Connected; }

    auto isDisconnected() const
        -> bool { return state_ == Disconnected; }

    // return true if success.
    auto getTcpInfo(struct tcp_info*) const
        -> bool;
    auto getTcpInfoString() const
        -> std::string;

    // 发送数据, thread safe cause it the data only sends in owner loop thread
    void send(std::span<char> data);
    void send(std::string_view message);
    void send(Buffer buf);
    void send(std::string message);
    template <typename Integer>
        requires(std::is_integral_v<Integer>)
    void send(Integer integer)
    {

        if (state_ == Connected)
        {
            if (owner_loop_->inOwnerThread())
            {
                sendInOwnerLoop_(&integer, sizeof(integer));
            }
            else
            {
                auto send_msg_task = [tcp_conn = shared_from_this(), integer]() -> void {
                    tcp_conn->sendInOwnerLoop_(&integer, sizeof(integer));
                };
                owner_loop_->runTask(send_msg_task);
            }
        }
    }

    // 关闭连接, NOT thread safe, no simultaneous calling
    void shutdown();
    void forceClose();
    void forceCloseWithDelay(double seconds);
    void SetTcpNoDelay(bool on);
    // reading or not

    void startRead();
    void stopRead();
    auto IsReading() const
        -> bool { return reading_; }; // NOT thread safe, may race with start/stopReadInLoop

    void setContext(const std::any& context)
    {
        context_ = context;
    }

    auto getContext() const
        -> const std::any&
    {
        return context_;
    }

    auto getMutableContext()
        -> std::any&
    {
        return context_;
    }
    void setConnetionCallback(const ConnectionCallback& cb)
    {
        connection_callback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb)
    {
        msg_callback_ = cb;
    }

    /**
     * @brief writeCompleteCallback_ 不是针对单条消息，而是针对 整个发送缓冲区变空。如果你连续 send() 多条消息，可能最后只触发一次回调。如果想按应用层“消息粒度”确认发送完毕，可以在上层自己维护一个队列：每条消息入队,发送时依次 append 到 outputBuffer.在 writeCompleteCallback_ 时，知道“队列的消息都发完了”。
     */
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    {
        write_complete_callback_ = cb;
    }

    /**
     * @brief set a callback for the connection close
     */
    void setCloseCallback(const CloseCallback& cb)
    {
        close_callback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    {
        high_watermark_callback_ = cb;
        high_watermark_          = highWaterMark;
    }
};