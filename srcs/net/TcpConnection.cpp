#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <functional>
#include <netinet/tcp.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>

#include "net/Buffer.h"
#include "net/Channel.h"
#include "net/Socket.h"
#include "net/TcpConnection.h"
#include "net/Timestamp.h"
#include "net/WeakCallback.h"
#include "net/Socketsops.h"

#include "logger/EasyLog.h"

void defaultConnectionCallback(const TcpConnectionPtr& conn)
{
    EASY_TRACE("defaultConnectionCallback: {}->{} is {}", conn->getLocalAddress().toIpPortRepr(), conn->getPeerAddress().toIpPortRepr(), conn->isConnected() ? "UP" : "DOWN");
}

void defaultMessageCallback(const TcpConnectionPtr&,
                            Buffer& buf,
                            Timestamp)
{
    buf.readAllAndDiscard();
}

static auto requiresNonNull(EventLoop* loop)
    -> EventLoop*
{
    if (loop == nullptr)
    {
        std::terminate();
        // LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

static constexpr auto c_highwater_mark = static_cast<const size_t>(64 * 1024 * 1024); // 64M

TcpConnection::TcpConnection(EventLoop* loop,
                             std::string name,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : owner_loop_ {requiresNonNull(loop)}
    , name_ {std::move(name)}
    , state_ {Connecting}
    , reading_ {true}
    , socket_ {new Socket(sockfd)}
    , socket_channel_ {new Channel {loop, sockfd}}
    , local_addr_ {localAddr}
    , peer_addr_ {peerAddr}
    , high_watermark_ {c_highwater_mark} // 64M
{

    socket_channel_->setHandler(this);
    // 注册读写等事件的回调
    // 这里直接捕获 this 是因为其生命周期要长于socket_channel for it`s TcpConnection`s member
    // socket_channel_->setReadCallback([this](Timestamp) {
    //     this->handleRead_(Timestamp::now());
    // });

    // socket_channel_->setWriteCallback([this]() {
    //     this->handleWrite_();
    // });
    // socket_channel_->setErrorCallback([this]() {
    //     this->handleError_();
    // });
    // socket_channel_->setCloseCallback([this]() {
    //     this->handleClose_();
    // });
    // socket_channel_.

    // todo: log
    //  LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    owner_loop_->assertInOwnerThread();
    LOG_INFO_FMT(log, "TcpConnection::dtor[{}] at fd={} state={}", name_.c_str(), socket_channel_->getFd(), (int)state_);
    assert(state_ == Disconnected);
}

auto TcpConnection::getTcpInfo(struct tcp_info* tcpi) const
    -> bool
{
    return socket_->getTcpInfo(tcpi);
}

auto TcpConnection::getTcpInfoString() const
    -> std::string
{
    auto buf = std::array<char, 1024> {};
    buf[0]   = '\0';
    socket_->getTcpInfoString(buf.data(), sizeof buf);
    return buf.data();
}

void TcpConnection::send(std::span<char> data)
{
    send(std::string_view {data.data(), data.size()});
}

void TcpConnection::send(std::string_view message)
{
    // only send when connected
    if (state_ == Connected)
    {
        if (owner_loop_->inOwnerThread())
        {
            sendInOwnerLoop_(message);
        }
        else
        {
            auto send_msg_task = [tcpconn = this, message = std::string {message}]() {
                tcpconn->sendInOwnerLoop_(message);
            };
            owner_loop_->runInLoop(send_msg_task);
        }
    }
}

void TcpConnection::send(Buffer buf)
{
    if (state_ == Connected)
    {
        if (owner_loop_->inOwnerThread())
        {
            sendInOwnerLoop_(buf.getReadableSV());
        }
        else
        {
            auto send_data_task = [tcp_conn = shared_from_this(), buf = std::move(buf)]() {
                tcp_conn->sendInOwnerLoop_(buf.getReadableSV());
            };
            owner_loop_->runInLoop(send_data_task);
        }
    }
}

void TcpConnection::send(std::string message)
{
    if (state_ == Connected)
    {
        if (owner_loop_->inOwnerThread())
        {
            sendInOwnerLoop_(message);
        }
        else
        {
            owner_loop_->runInLoop([guard = shared_from_this(), message = std::move(message)]() -> void {
                guard->sendInOwnerLoop_(message);
            });
        }
    }
}

void TcpConnection::sendInOwnerLoop_(std::string_view message)
{
    sendInOwnerLoop_(message.data(), message.size());
}
/**
 * 发送数据 应用写的快 而内核发送数据慢 需要把待发送数据写入缓冲区，而且设置了水位回调
 **/
void TcpConnection::sendInOwnerLoop_(const void* data, size_t len)
{
    owner_loop_->assertInOwnerThread();

    // 对于 send() -> queueTask(), 有可能在EventLoop doPendingTask() 前，
    // 该TcpConnection已经关闭
    if (state_ == Disconnected)
    {
        LOG_WARN_FMT(log, "disconnected, give up writing");
        return;
    }

    auto nwrote      = ssize_t {0};
    auto remaining   = len;
    auto fault_error = false;

    // if no thing in output queue, try writing directly
    if (not socket_channel_->isWriting() and output_buf_.getReadableBytesCount() == 0)
    {
        nwrote = ::write(socket_channel_->getFd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && write_complete_callback_) // 全部发送完毕
            {
                // why queue in loop here instead of call write_complete_callback_ directly?
                // Answer: https://github.com/chenshuo/muduo/discussions/560
                owner_loop_->queueInLoop([tcpconn = shared_from_this()]() {
                    tcpconn->write_complete_callback_(tcpconn);
                });
            }
        }
        else // nwrote < 0, 错误处理
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) // EWOULDBLOCK表示非阻塞情况下没有数据后的正常返回 等同于EAGAIN
            {
                // LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                {
                    fault_error = true;
                }
            }
        }
    }
    assert(remaining <= len);
    /**
     * 说明当前这一次write并没有把数据全部发送出去 剩余的数据需要保存到缓冲区当中
     * 然后给channel注册EPOLLOUT事件，Poller发现tcp的发送缓冲区有空间后会通知
     * 相应的sock->channel，调用channel对应注册的writeCallback_回调方法，
     * channel的writeCallback_实际上就是TcpConnection设置的handleWrite回调，
     * 把发送缓冲区outputBuffer_的内容全部发送完成
     **/
    if (not fault_error && remaining > 0)
    {
        // 目前发送缓冲区剩余的待发送的数据的长度
        auto in_obuf = output_buf_.getReadableBytesCount();
        // 第二个条件用于判断，第二次send时再次达到highWaterMark
        // 同时保证一次send只能触发一次highWaterMark
        if (in_obuf + remaining >= high_watermark_
            and in_obuf < high_watermark_
            and high_watermark_callback_) // 待发送数据超过了高水位
        {
            owner_loop_->queueInLoop([tcpconn = shared_from_this(), watermark_now = in_obuf + remaining]() {
                tcpconn->high_watermark_callback_(tcpconn, watermark_now);
            });
        }

        output_buf_.append((char*)data + nwrote, remaining);
        if (not socket_channel_->isWriting())
        {
            // 注册 channel 的写事件
            socket_channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{

    if (auto expected = Connected; state_.compare_exchange_strong(expected, Disconnecting))
    {
        // how about weak ptr here?
        owner_loop_->runInLoop([tcpconn = shared_from_this()] {
            tcpconn->shutdownInOwnerLoop_();
        });
    }
}

void TcpConnection::shutdownInOwnerLoop_()
{
    owner_loop_->assertInOwnerThread();
    // if 当前outputBuffer_中没有待发送的数据了 则直接关闭写端
    if (not socket_channel_->isWriting())
    {
        socket_->shutdownWrite();
    }
}

void TcpConnection::initInLoop_()
{
    owner_loop_->assertInOwnerThread();
    assert(state_ == Connecting);
    setState_(Connected);
    socket_channel_->enableReading();
    connection_callback_(shared_from_this());
}

void TcpConnection::destructConnectionInOnwerLoop_()
{
    owner_loop_->assertInOwnerThread();
}
// 连接建立

void TcpConnection::forceClose()
{
    // FIXME: use compare and swap
    if (state_ == Connected || state_ == Disconnecting)
    {
        setState_(Disconnecting);
        owner_loop_->queueInLoop([tcpconn = shared_from_this()] { tcpconn->forceCloseInOwnerLoop_(); });
    }
}

void TcpConnection::forceCloseWithDelay(double seconds)
{
    if (state_ == Connected || state_ == Disconnecting)
    {
        setState_(Disconnecting);
        owner_loop_->runAfter(
            seconds,
            makeWeakCallback(shared_from_this(),
                             &TcpConnection::forceClose)); // not forceCloseInLoop to avoid race condition
    }
}

void TcpConnection::forceCloseInOwnerLoop_()
{
    owner_loop_->assertInOwnerThread();
    if (state_ == Connected || state_ == Disconnecting)
    {
        // as if we received 0 byte in handleRead();
        handleClose_();
    }
}

auto TcpConnection::stateToString_() const
    -> std::string_view
{
    switch (state_)
    {
        case Disconnected:
            return "kDisconnected";
        case Connecting:
            return "kConnecting";
        case Connected:
            return "kConnected";
        case Disconnecting:
            return "kDisconnecting";
        default:
            return "unknown state";
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->SetTcpNoDelay(on);
}

void TcpConnection::startRead()
{
    owner_loop_->runInLoop([this] { startReadInOwnerLoop_(); });
}

void TcpConnection::startReadInOwnerLoop_()
{
    owner_loop_->assertInOwnerThread();
    if (!reading_ || !socket_channel_->isReading())
    {
        socket_channel_->enableReading();
        reading_ = true;
    }
}

void TcpConnection::stopRead()
{
    owner_loop_->runInLoop([this] { stopReadInOwnerLoop_(); });
}

void TcpConnection::stopReadInOwnerLoop_()
{
    owner_loop_->assertInOwnerThread();
    if (reading_ || socket_channel_->isReading())
    {
        socket_channel_->diableReading();
        reading_ = false;
    }
}

void TcpConnection::handleRead_(Timestamp receiveTime)
{

    owner_loop_->assertInOwnerThread();
    auto saved_errno = 0;
    auto n           = input_buf_.readFd(socket_channel_->getFd(), &saved_errno);
    if (n > 0) // 有数据到达
    {
        // 调用用户 TcpServer 设置的回调操作设置的 MessageCallback
        msg_callback_(shared_from_this(), input_buf_, receiveTime);
    }
    else if (n == 0) //  socket对端关闭
    {
        handleClose_();
    }
    else // 错误发生
    {
        errno = saved_errno;
        EASY_ERROR("TcpConnection::socketChannelReadCB_");
        handleError_();
    }
}

void TcpConnection::handleWrite_()
{
    owner_loop_->assertInOwnerThread();
    // muduo 不完整支持 half-open connection，详见https://github.com/chenshuo/muduo/discussions/588
    if (socket_channel_->isWriting())
    {
        auto saved_errno = 0;
        auto n           = output_buf_.writeFd(socket_channel_->getFd(), &saved_errno);
        if (n > 0)
        {
            if (output_buf_.getReadableBytesCount() == 0) // 输出缓冲区发送完毕
            {
                // 不再关注写事件,否则造成poller忙等待
                socket_channel_->diableWriting();
                if (write_complete_callback_)
                {
                // why queue in loop here instead of call write_complete_callback_ directly?
                // Answer: https://github.com/chenshuo/muduo/discussions/560
                    owner_loop_->runInLoop([conn = shared_from_this()]() {
                        conn->write_complete_callback_(conn);
                    });
                }
                if (state_ == Disconnecting) // 如果正在关闭连接
                {
                    // 在当前所属的loop中把TcpConnection删除掉
                    shutdownInOwnerLoop_();
                }
            }
        }
        else
        {
            EASY_ERROR("{}", strerror(saved_errno));
        }
    }
    else
    {
        EASY_ERROR("TcpConnection fd=%d is down, no more writing", socket_channel_->getFd());
    }
}

void TcpConnection::handleClose_()
{
    EASY_INFO("TcpConnection::handleClose fd=%d state=%d", socket_channel_->getFd(), (int)state_);
    owner_loop_->assertInOwnerThread();
    assert(state_ == Disconnecting or state_ == Connected);
    // we don't close fd, leave it to dtor, so we can find leaks easily.
    setState_(Disconnected);

    socket_channel_->diableAll();
    socket_channel_->remove();
    // 确保回调期间对象存活
    // so must be the last line
    auto guard_this = shared_from_this();
    close_callback_(guard_this);
}

void TcpConnection::handleError_()
{
    auto err = 0;
    // 必须通过::getsockopt获取错误，否则会一直触发EPOLLERR事件
    err = Sock::getSocketError(socket_channel_->getFd());
    EASY_ERROR("TcpConnection::handleError name:{} - SO_ERROR:{}\n", name_, err);
}