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

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <memory>

static auto log = GET_ROOT_LOGGER();

Connector::Connector(EventLoop* loop, const InetAddress& peerAddr)
    : loop_ {loop}
    , peer_addr_ {peerAddr}
    , in_connecting_ {false}
    , state_ {Disconnected}
    , retry_delay_ms_ {c_init_retry_delay_ms}
    , retry_timer_id_ {0}
// 数据成员中其实是有Channel对象的,但却并没有初始化,因为在连接成功的时候才能有一个有效的fd,那时才可以创建一个有效的Channel
{
    LOG(log, "ctor[{}]", std::bit_cast<uint64_t>(this));
}

Connector::~Connector()
{
    LOG(log, "dtor[{}]", std::bit_cast<uint64_t>(this));
    assert(channel_ == nullptr);
}

void Connector::start()
{
    if (not in_connecting_.exchange(true))
    {
        // 线程的安全性由其所属的 tcpclient 保证
        loop_->runTask([this] { this->startInLoop_(); });
    }
}

void Connector::startInLoop_()
{
    loop_->assertInOwnerThread();
    assert(state_ == Disconnected);
    if (in_connecting_)
    {
        connectPeer_();
    }
    else
    {
        LOG(log, "do not connect");
    }
}

void Connector::stop()
{
    if (in_connecting_.exchange(false))
    {
        loop_->queueTask([this] {
            this->stopInLoop_();
        });
    }
    // FIXME: cancel timer
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
        LOG<LogLevel::DEBUG>(log, "stop---");
    }
}

void Connector::connectPeer_()
{
    auto sockfd      = Sock::createNonblockingOrDie(peer_addr_.getFamily());
    auto ret         = Sock::connect(sockfd, *peer_addr_.getSockAddr());
    auto saved_errno = (ret == 0) ? 0 : errno;
    switch (saved_errno)
    {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            postSocketConnected_(sockfd);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry_(sockfd);
            break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG<LogLevel::SYSERR>(log, "connect error in Connector::startInLoop {} {}", saved_errno, strerror(saved_errno));
            Sock::close(sockfd);
            break;

        default:
            LOG<LogLevel::SYSERR>(log, "Unexpected error in Connector::startInLoop {}", saved_errno);
            Sock::close(sockfd);
            // connectErrorCallback_();
            break;
    }
}

void Connector::restart()
{
    loop_->assertInOwnerThread();
    setState_(Disconnected);
    retry_delay_ms_ = c_init_retry_delay_ms;
    in_connecting_  = true;
    startInLoop_();
}

void Connector::postSocketConnected_(int sockfd)
{
    setState_(Connecting);
    assert(channel_ == nullptr);
    channel_ = std::make_unique<Channel>(loop_, sockfd);
    channel_->setWriteCallback(
        [this] { this->channelWriteCB_(); });
    channel_->setErrorCallback(
        [this] { this->channelErrorCB_(); });

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
    loop_->queueTask([this] { this->resetChannel_(); });
    return sockfd;
}

void Connector::resetChannel_()
{
    channel_.reset();
}

void Connector::channelWriteCB_()
{
    LOG<LogLevel::TRACE>(log, "Connector::channelWriteCB_ {}", static_cast<int>(state_));

    if (state_ == Connecting)
    {
        auto sockfd = removeAndResetChannel_();
        auto err    = Sock::getSocketError(sockfd);
        if (err)
        {
            LOG<LogLevel::WARN>(log, "Connector::handleWrite - SO_ERROR = {} {}", err, strerror(err));
            retry_(sockfd);
        }
        else if (Sock::isSelfConnect(sockfd))
        {
            LOG<LogLevel::WARN>(log, "Connector::handleWrite - Self connect");
            retry_(sockfd);
        }
        else
        {
            setState_(Connected);
            if (in_connecting_)
            {
                new_connection_callback_(sockfd);
            }
            else
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
    LOG<LogLevel::ERROR>(log, "Connector::handleError state={}", static_cast<int>(state_.load()));
    if (state_ == Connecting)
    {
        auto sockfd = removeAndResetChannel_();
        auto err    = Sock::getSocketError(sockfd);
        // todo:log
        LOG<LogLevel::TRACE>(log, "Connector::handleError - SO_ERROR = {} {}", err, strerror(err));
        retry_(sockfd);
    }
}

void Connector::retry_(int sockfd)
{
    Sock::close(sockfd);
    setState_(Disconnected);
    if (in_connecting_)
    {
        // 一定间隔后充实
        LOG<LogLevel::INFO>(log, "Connector::retry - Retry connecting to {} in {} milliseconds.", peer_addr_.toIpPortRepr(), retry_delay_ms_);
        retry_timer_id_ = loop_->runAfter(retry_delay_ms_ / 1000.0,
                                          [this] { this->startInLoop_(); });
        retry_delay_ms_ = std::min(retry_delay_ms_ * 2, c_max_retry_delay_ms);
    }
    else
    {
        LOG<LogLevel::DEBUG>(log, "do not connecting");
    }
}
