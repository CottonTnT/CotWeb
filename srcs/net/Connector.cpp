// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include "net/Connector.h"

#include "logger/LogLevel.h"
#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Socketsops.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"
#include "net/TcpClient.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <memory>
#include <utility>

static auto log = GET_ROOT_LOGGER();

Connector::Connector(EventLoop* loop, const InetAddress& peerAddr, std::weak_ptr<TcpClient> owner)
    : loop_ {loop}
    , peer_addr_ {peerAddr}
    , onwner_ {std::move(owner)}
    , is_connect_canceled_ {true}
    , state_ {Disconnected}
    , retry_delay_ms_ {c_init_retry_delay_ms}
    , retry_timer_id_ {0}
// 数据成员中其实是有Channel对象的,但却并没有初始化,因为在连接成功的时候才能有一个有效的fd,那时才可以创建一个有效的Channel
{
    LOG_DEBUG_FMT(log, "ctor[{}]", std::bit_cast<uint64_t>(this));
}

Connector::~Connector()
{
    LOG_DEBUG_FMT(log, "dtor[{}]", std::bit_cast<uint64_t>(this));
    assert(channel_ == nullptr);
}

void Connector::start()
{
    // is_connect_canceled = false;
    // 线程的安全性由其所属的 tcpclient 保证
    loop_->runTask([this, weakOwner= onwner_] {
        if (weakOwner.lock())
        {
            this->startInLoop_();
        }
    });
}

void Connector::startInLoop_()
{
    loop_->assertInOwnerThread();
    assert(state_ == Disconnected);

    // connectPeer_();
    if (not is_connect_canceled_)
    {
        connectPeer_();
    }
    else
    {
        LOG_DEBUG_FMT(log, "do not connect");
    }
}

void Connector::stop()
{
    is_connect_canceled_ = true;
    {
        loop_->queueTask([this, weakOwner = onwner_] {
            if (auto owner = weakOwner.lock(); owner != nullptr)
            {
                this->stopInLoop_();
            }
        });
    }
}

void Connector::stopInLoop_()
{
    loop_->assertInOwnerThread();

    if (retry_timer_id_ != 0)
    {
        loop_->cancelTimer(retry_timer_id_);
        retry_timer_id_ = 0;
    }

    if (state_ == Connecting) // 如果正在连接
    {
        setState_(Disconnected);
        auto sockfd = removeAndResetChannel_();

        Sock::close(sockfd);
        LOG_DEBUG_FMT(log, "stoping");
    }
}

void Connector::connectPeer_()
{
    auto sockfd = Sock::createNonblockingOrDie(peer_addr_.getFamily());
    // 这里是非阻塞的
    auto ret         = Sock::connect(sockfd, *peer_addr_.getSockAddr());
    auto saved_errno = (ret == 0) ? 0 : errno;
    switch (saved_errno)
    {
        // 可继续连接的情况
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            LOG_INFO_FMT(log, "SockConnect established in Connector:startInLoop [sockfd-{}]", sockfd);
            postSocketConnected_(sockfd);
            break;

        // 可重试的错误
        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry_(sockfd);
            break;

        // 程序/权限/参数错误
        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_SYSERR_FMT(log, "connect error in Connector::startInLoop {} {}", saved_errno, strerror(saved_errno));
            Sock::close(sockfd);
            break;

        default:
            LOG_SYSERR_FMT(log, "Unexpected error in Connector::startInLoop {}", saved_errno);
            Sock::close(sockfd);
            // connectErrorCallback_();
            break;
    }
}

void Connector::restart()
{
    loop_->assertInOwnerThread();
    setState_(Disconnected);
    retry_delay_ms_     = c_init_retry_delay_ms;
    is_connect_canceled_ = true;
    startInLoop_();
}

void Connector::postSocketConnected_(int sockfd)
{
    setState_(Connecting);
    assert(channel_ == nullptr);
    channel_ = std::make_unique<Channel>(loop_, sockfd);
    channel_->setWriteCallback(
        [this, weakOwner = onwner_] {
            if (auto owner = weakOwner.lock(); owner != nullptr)
            {

                this->channelWriteCB_();
            }
        });
    channel_->setErrorCallback(
        [this, weakOwner = onwner_] {
            if (auto owner = weakOwner.lock(); owner != nullptr)
            {
                this->channelErrorCB_();
            }
        });

    // channel_->tie(shared_from_this());
    channel_->enableWriting();
}

auto Connector::removeAndResetChannel_()
    -> int
{
    channel_->unregisterAllEvent();
    channel_->remove();
    auto sockfd = channel_->getFd();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    // 为什么这里可以这样做，因为 eventloop will doPendingTask after handleEvent in same poll
    loop_->queueTask([this] { this->resetChannel_(); });
    return sockfd;
}

void Connector::resetChannel_()
{
    channel_.reset();
}

void Connector::channelWriteCB_()
{
    LOG_TRACE_FMT(log, "Connector::channelWriteCB_ {}", static_cast<int>(state_));

    if (state_ == Connecting)
    {
        auto sockfd = removeAndResetChannel_();
        auto err    = Sock::getSocketError(sockfd);
        if (err)
        {
            LOG_WARN_FMT(log, "Connector::handleWrite - SO_ERROR = {} {}", err, strerror(err));
            retry_(sockfd);
        }
        else if (Sock::isSelfConnect(sockfd))
        {
            LOG_WARN_FMT(log, "Connector::handleWrite - Self connect");
            retry_(sockfd);
        }
        else
        {
            setState_(Connected);
            // 是否用户已经取消连接
            if (not is_connect_canceled_)
            {
                new_connection_callback_(sockfd);
            }
            else // 防止幽灵连接
            {
                Sock::close(sockfd);
            }
        }
    }
    else
    {
        // what happened?
        assert(state_ == Disconnected);
    }
}

void Connector::channelErrorCB_()
{
    LOG_ERROR_FMT(log, "Connector::handleError state={}", static_cast<int>(state_.load()));
    if (state_ == Connecting)
    {
        auto sockfd = removeAndResetChannel_();
        auto err    = Sock::getSocketError(sockfd);
        LOG_TRACE_FMT(log, "Connector::handleError - SO_ERROR = {} {}", err, strerror(err));
        retry_(sockfd);
    }
}

void Connector::retry_(int sockfd)
{
    Sock::close(sockfd);
    setState_(Disconnected);
    if (is_connect_canceled_)
    {
        // 一定间隔后充实
        LOG_TRACE_FMT(log, "Connector::retry - Retry connecting to {} in {} milliseconds.", peer_addr_.toIpPortRepr(), retry_delay_ms_);
        retry_timer_id_ = loop_->runAfter(retry_delay_ms_ / 1000.0,
                                          [this, weakOwner = onwner_] {
                                              if (auto owner = weakOwner.lock(); owner != nullptr)
                                              {

                                                  this->startInLoop_();
                                              }
                                          });
        retry_delay_ms_ = std::min(retry_delay_ms_ * 2, c_max_retry_delay_ms);
    }
    else
    {
        LOG_DEBUG_FMT(log, "do not connecting");
    }
}
