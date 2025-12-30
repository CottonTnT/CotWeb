// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include "net/Connector.h"

#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Socketsops.h"
#include "net/TcpClient.h"
#include "logger/EasyLog.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <memory>
#include <latch>
#include <sys/stat.h>

Connector::Connector(EventLoop* loop, const InetAddress& peerAddr)
    : loop_ {loop}
    , peer_addr_ {peerAddr}
    , state_ {Disconnected}
    , channel_ {nullptr}
    , sock_fd_ {-1}
    , retry_delay_ms_ {c_init_retry_delay_ms}
    , retry_timer_id_ {0}
// 数据成员中其实是有Channel对象的,但却并没有初始化,因为在连接成功的时候才能有一个有效的fd,那时才可以创建一个有效的Channel
{
    EASY_DEBUG("ctor[{}]", std::bit_cast<uint64_t>(this));
}

Connector::~Connector()
{
    EASY_DEBUG("dtor[{}]", std::bit_cast<uint64_t>(this));
    auto latch = std::latch {1};
    loop_->runInLoop([this, &latch]() {
        loop_->assertInOwnerThread();
        if (channel_)
        {
            stopInLoop_();
        }
        latch.count_down();
    });
    latch.wait();
}

void Connector::start()
{
    TRACE();
    loop_->runInLoop([this] {
        this->startInLoop_();
    });
}

void Connector::startInLoop_()
{
    TRACE();
    loop_->assertInOwnerThread();
    if (state_ == Disconnected)
    {
        connectPeer_();
    }
    else if (state_ == Connecting or state_ == Connected) // 幂等
    {
        EASY_WARN("Connector: do not start again-{}", static_cast<int>(state_));
    }
}

void Connector::stop()
{
    TRACE();
    loop_->queueInLoop([this] {
        this->stopInLoop_();
    });
}
void Connector::cancelReryTimer_()
{
    TRACE();
    loop_->assertInOwnerThread();
    if (retry_timer_id_ != 0)
    {
        EASY_INFO("[Connector::cancelReryTimer_] : timerId = {}", retry_timer_id_);
        loop_->cancelTimer(retry_timer_id_);
        retry_timer_id_ = 0;
    }
    else
    {
        EASY_WARN("[Connector::cancelReryTimer_] : timerId = {}", retry_timer_id_);
    }
}
void Connector::stopInLoop_()
{
    TRACE();
    loop_->assertInOwnerThread();
    if (state_ == Connecting) // 如果正在连接
    {
        resetConnector_(false);
        EASY_DEBUG("stop connect");
    }
    else
    {
        EASY_WARN("[Connector::stopInLoop_] : Connector state expect Connecting while {}", static_cast<int>(state_));
    }
}

void Connector::connectPeer_()
{
    TRACE();
    assert(state_ == Disconnected);
    loop_->assertInOwnerThread();
    sock_fd_ = Sock::createNonblockingOrDie(peer_addr_.getFamily());
    // 这里是非阻塞的
    auto ret         = Sock::connect(sock_fd_, *peer_addr_.getSockAddr());
    auto saved_errno = (ret == 0) ? 0 : errno;
    switch (saved_errno)
    {
        // 可继续连接的情况
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            EASY_INFO("SockConnect established in Connector:startInLoop [sockfd-{}]", sock_fd_);
            initConnectChannel_(sock_fd_);
            break;

        // 可重试的错误
        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry_(false);
            break;

        // 程序/权限/参数错误
        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            EASY_SYSERR("connect error in Connector::startInLoop {} {}", saved_errno, strerror(saved_errno));
            resetSockFd_();
            break;

        default:
            EASY_SYSERR("Unexpected error in Connector::startInLoop {}", saved_errno);
            resetSockFd_();
            // connectErrorCallback_();
            break;
    }
}

void Connector::restart()
{
    TRACE();
    loop_->runInLoop([this]() {
        this->restartInLoop_();
    });
}
void Connector::restartInLoop_()
{
    TRACE();
    loop_->assertInOwnerThread();
    if (state_ == Connecting)
    {
        stopInLoop_();
    }
    else if (state_ == Connected)
    {
        setState_(Disconnected);
    }
    startInLoop_();
}

void Connector::initConnectChannel_(int sockfd)
{
    TRACE();
    assert(channel_ == nullptr);
    assert(state_ == Disconnected);
    setState_(Connecting);
    channel_ = std::make_unique<Channel>(loop_, sockfd);
    channel_->setHandler(this);
    // channel_->setWriteCallback([this] {
    //     this->handleWrite_();
    // });
    // channel_->setErrorCallback([this] {
    //     this->handleError_();
    // });
    channel_->enableWriting();
}

auto Connector::removeAndResetChannel_(bool delayReset)
    -> void
{
    TRACE();
    if (channel_ == nullptr)
    {
        return;
    }
    channel_->diableAll();
    channel_->remove();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    // 为什么这里可以这样做，因为 eventloop will doPendingTask after handleEvent in same poll
    if (delayReset)
    {
        loop_->queueInLoop([this] { this->channel_.reset(); });
    }
    else
    {
        channel_.reset();
    }
}

void Connector::handleWrite_()
{
    TRACE();
    switch (state_)
    {
        case Connecting:{
            auto err = Sock::getSocketError(sock_fd_);
            if (err)
            {
                EASY_WARN("Connector::handleWrite - SO_ERROR = {} {}", err, strerror(err));
                retry_(true);
            }
            else if (Sock::isSelfConnect(sock_fd_))
            {
                EASY_WARN("Connector::handleWrite - Self connect");
                retry_(true);
            }
            else
            {
                // 此时算是连接已建立，Connector 不再管理该 channel，直接重置
                removeAndResetChannel_(true);
                setState_(Connected);
                new_connection_callback_(sock_fd_);
            }
            break;
        }
        default:
            //EPOLLERR | EPOLLOUT
            assert(state_ == Disconnected);
            break;
    }
}

void Connector::handleError_()
{
    EASY_ERROR("Connector::handleError state={}", static_cast<int>(state_));
    if (state_ == Connecting)
    {
        auto err = Sock::getSocketError(sock_fd_);
        EASY_TRACE("Connector::handleError - SO_ERROR = {} {}", err, strerror(err));
        retry_(true);
    }
}

void Connector::resetSockFd_()
{
    TRACE();
    if (sock_fd_ >= 0)
    {
        Sock::close(sock_fd_);
        sock_fd_ = -1;
    }
}

void Connector::resetConnector_(bool delayResetChannel)
{
    TRACE();
    loop_->assertInOwnerThread();
    setState_(Disconnected);
    cancelReryTimer_();
    removeAndResetChannel_(delayResetChannel);
    resetSockFd_();
}

void Connector::retry_(bool delayResetChannel)
{
    TRACE();
    assert(state_ != Disconnected);
    loop_->assertInOwnerThread();
    // 取消旧的定时器, 防止重复调用
    resetConnector_(delayResetChannel);
    // 一定间隔后充实
    EASY_INFO("Connector::retry - Retry connecting to {} in {} milliseconds.", peer_addr_.toIpPortRepr(), retry_delay_ms_);
    retry_timer_id_ = loop_->runAfter(retry_delay_ms_ / 1000.0, [this] {
        this->startInLoop_();
    });
    retry_delay_ms_ = std::min(retry_delay_ms_ * 2, c_max_retry_delay_ms);
}
