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

#include <cassert>
#include <cerrno>

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop)
    , server_addr_(serverAddr)
    , start_connect_(false)
    , state_(kDisconnected)
    , retry_delay_ms_(c_init_retry_delay_ms)
// 数据成员中其实是有Channel对象的,但却并没有初始化,因为在连接成功的时候才能有一个有效的fd,那时才可以创建一个有效的Channel
{
    // todo:log
    //  LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
    // todo:log
    //  LOG_DEBUG << "dtor[" << this << "]";
    assert(!channel_);
}

void Connector::start()
{
    start_connect_ = true;
    loop_->runTask([this] { startInLoop_(); }); // FIXME: unsafe
    // loop_->RunInOwnerThread([connector = shared_from_this()] { connector->StartInLoop_(); }); // FIXME: unsafe
}

void Connector::startInLoop_()
{
    loop_->assertInOwnerThread();
    assert(state_ == kDisconnected);
    if (start_connect_)
    {
        connect_();
    }
    else
    {
        // todo:log
        // LOG_DEBUG << "do not connect";
    }
}

void Connector::stop()
{
    start_connect_ = false;
    loop_->queueTask([this] { stopInLoop_(); }); // FIXME: unsafe
                                                          // FIXME: cancel timer
}

void Connector::stopInLoop_()
{
    loop_->assertInOwnerThread();
    if (state_ == kConnecting)
    {
        setState_(kDisconnected);
        auto sockfd = removeAndResetChannel_();
        retry_(sockfd);
    }
}

void Connector::connect_()
{
    auto sockfd      = Sock::createNonblockingOrDie(server_addr_.getFamily());
    auto ret         = Sock::connect(sockfd, *server_addr_.getSockAddr());
    auto saved_errno = (ret == 0) ? 0 : errno;
    switch (saved_errno)
    {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            connecting_(sockfd);
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
            // todo:log
            //  LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
            Sock::close(sockfd);
            break;

        default:
            // todo:log
            //  LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
            Sock::close(sockfd);
            // connectErrorCallback_();
            break;
    }
}

void Connector::restart()
{
    loop_->assertInOwnerThread();
    setState_(kDisconnected);
    retry_delay_ms_ = c_init_retry_delay_ms;
    start_connect_        = true;
    startInLoop_();
}

void Connector::connecting_(int sockfd)
{
    setState_(kConnecting);
    assert(!channel_);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(
        [this] { HandleWrite_(); }); // FIXME: unsafe
    channel_->setErrorCallback(
        [this] { HandleError_(); }); // FIXME: unsafe

    // channel_->tie(shared_from_this()); is not working,
    // as channel_ is not managed by shared_ptr
    channel_->enableReading();
}

auto Connector::removeAndResetChannel_()
    -> int
{
    channel_->unregisterAllEvent();
    channel_->remove();
    auto sockfd = channel_->getFd();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueTask([connector = this] { connector->resetChannel_(); }); // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel_()
{
    channel_.reset();
}

void Connector::HandleWrite_()
{
    // todo:log
    //  LOG_TRACE << "Connector::handleWrite " << state_;

    if (state_ == kConnecting)
    {
        auto sockfd = removeAndResetChannel_();
        auto err    = Sock::getSocketError(sockfd);
        if (err)
        {
            // todo:log
            //  LOG_WARN << "Connector::handleWrite - SO_ERROR = "
            //           << err << " " << strerror_tl(err);
            retry_(sockfd);
        }
        else if (Sock::IsSelfConnect(sockfd))
        {
            // todo:log
            //  LOG_WARN << "Connector::handleWrite - Self connect";
            retry_(sockfd);
        }
        else
        {
            // todo:log
            setState_(kConnected);
            if (connect_)
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
        assert(state_ == kDisconnected);
    }
}

void Connector::HandleError_()
{
    // todo:log
    //  LOG_ERROR << "Connector::handleError state=" << state_;
    if (state_ == kConnecting)
    {
        auto sockfd = removeAndResetChannel_();
        auto err    = Sock::getSocketError(sockfd);
        // todo:log
        //  LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
        retry_(sockfd);
    }
}

void Connector::retry_(int sockfd)
{
    Sock::close(sockfd);
    setState_(kDisconnected);
    if (start_connect_)
    {
        // todo:log
        //  LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
        //   << " in " << retryDelayMs_ << " milliseconds. ";
        loop_->runAfter(retry_delay_ms_ / 1000.0,
                        [connector = shared_from_this()] { connector->startInLoop_(); });
        retry_delay_ms_ = std::min(retry_delay_ms_ * 2, c_max_retry_delay_ms);
    }
    else
    {
        // todo:log
        //  LOG_DEBUG << "do not connect";
    }
}
