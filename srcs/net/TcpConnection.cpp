#include <cassert>
#include <cstddef>
#include <exception>
#include <functional>
#include <netinet/tcp.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>

#include "net/Buffer.h"
#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Socket.h"
#include "net/TcpConnection.h"
#include "net/Timestamp.h"
#include "net/WeakCallback.h"

void defaultConnectionCallback(const TcpConnectionPtr& conn)
{
    // todo:log
    //    LOG_TRACE << conn->localAddress().toIpPort() << " -> "
    //              << conn->peerAddress().toIpPort() << " is "
    //              << (conn->connected() ? "UP" : "DOWN");
    //  do not call conn->forceClose(), because some users want to register message callback only.
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

    // 注册读写等事件的回调
    // 这里直接捕获 this 是因为其生命周期要长于socket_channel for it`s TcpConnection`s member
    socket_channel_->setReadCallback([this](Timestamp) {
        this->socketChannelReadCB_(Timestamp::Now());
    });

    socket_channel_->setWriteCallback([this]() {
        this->socketChannelWriteCB_();
    });
    socket_channel_->setErrorCallback([this]() {
        this->socketChannelErrorCB_();
    });
    socket_channel_->setCloseCallback([this]() {
        this->socketChannelCloseCB_();
    });

    // todo: log
    //  LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    // LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), channel_->GetFd(), (int)state_);
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
            owner_loop_->runTask(send_msg_task);
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
            owner_loop_->runTask(send_data_task);
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
            auto send_msg_task = [tcp_conn = shared_from_this(), message = std::move(message)]() -> void {
                tcp_conn->sendInOwnerLoop_(message);
            };
            owner_loop_->runTask(send_msg_task);
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

    // 之前调用过该connection的shutdown 不能再进行发送了
    if (state_ == Disconnected)
    {
        // todo:log
        // LOG_WARN("disconnected, give up writing");
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
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                auto write_complete_task = [tcpconn = shared_from_this()]() {
                    // here the "tcp->write..." make tcpconn will be valid in write_complete_callback_ unless the write_complete_callback_ will delete the tcpconn stupidly
                    tcpconn->write_complete_callback_(tcpconn);
                };
                owner_loop_->queueTask(write_complete_task);
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
        auto old_len = output_buf_.getReadableBytesCount();
        if (old_len + remaining >= high_watermark_ && old_len < high_watermark_ && high_watermark_callback_) // 待发送数据超过了高水位
        {
            auto high_watermark_task = [tcpconn = shared_from_this(), watermark_now = old_len + remaining]()
                -> void {
                tcpconn->high_watermark_callback_(tcpconn, watermark_now);
            };
            owner_loop_->queueTask(high_watermark_task);
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
        owner_loop_->runTask([tcpconn = shared_from_this()] {
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

void TcpConnection::postConnectionCreate_()
{
    owner_loop_->assertInOwnerThread();
    assert(state_ == Connecting);
    setState_(Connected);
    socket_channel_->tie(shared_from_this());
    socket_channel_->enableReading();

    connection_callback_(shared_from_this());
}

void TcpConnection::destroyConnection_()
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
        owner_loop_->queueTask([tcpconn = shared_from_this()] { tcpconn->forceCloseInOwnerLoop_(); });
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
        socketChannelCloseCB_();
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

void TcpConnection::SetTcpNoDelay(bool on)
{
    socket_->SetTcpNoDelay(on);
}

void TcpConnection::startRead()
{
    owner_loop_->runTask([this] { startReadInOwnerLoop_(); });
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
    owner_loop_->runTask([this] { stopReadInOwnerLoop_(); });
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

// 读是相对服务器而言的 当对端客户端有数据到达 服务器端检测到EPOLLIN 就会触发该fd上的回调 handleRead 取读走对端发来的数据
void TcpConnection::socketChannelReadCB_(Timestamp receiveTime)
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
        socketChannelCloseCB_();
    }
    else // 错误发生
    {
        errno = saved_errno;
        // LOG_ERROR("TcpConnection::handleRead");
        socketChannelErrorCB_();
    }
}

void TcpConnection::socketChannelWriteCB_()
{
    owner_loop_->assertInOwnerThread();
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
                    // TcpConnection对象在其所在的subloop中 向 pendingFunctors_ 中加入回调
                    // why not handle it right now?
                    // make sure that tcpconnection only handle the IO, and unify call time of all callback in the eventloop
                    owner_loop_->queueTask([tcpconn = shared_from_this()]() {
                        tcpconn->write_complete_callback_(tcpconn);
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
            // todo: log
            //  LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        // todo:log
        //  LOG_ERROR("TcpConnection fd=%d is down, no more writing", channel_->GetFd());
    }
}

void TcpConnection::socketChannelCloseCB_()
{
    // todo:log
    //  LOG_INFO("TcpConnection::handleClose fd=%d state=%d\n", channel_->GetFd(), (int)state_);
    owner_loop_->assertInOwnerThread();
    assert(state_ == Disconnecting or state_ == Connected);
    // we don't close fd, leave it to dtor, so we can find leaks easily.
    setState_(Disconnected);

    socket_channel_->unregisterAllEvent();
    socket_channel_->remove();
    // 确保回调期间对象存活
    // so must be the last line
    auto guard_this = shared_from_this();
    close_callback_(guard_this);
}

void TcpConnection::socketChannelErrorCB_()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    auto err         = 0;
    if (::getsockopt(socket_channel_->getFd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    // todo: log
    //  LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}