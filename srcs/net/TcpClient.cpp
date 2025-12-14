// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

// #include "muduo/base/Logging.h"
#include "logger/LogLevel.h"
#include "net/Callbacks.h"
#include "net/Connector.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/Socketsops.h"
#include "net/TcpConnection.h"
#include "net/TcpClient.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"

#include <bit>
#include <memory>

static auto log = GET_ROOT_LOGGER();

TcpClient::TcpClient(EventLoop* loop,
                     const InetAddress& serverAddr,
                     std::string nameArg)
    : loop_ {loop}
    , name_ {std::move(nameArg)}
    , connection_callback_ {defaultConnectionCallback}
    , conn_close_callback_ {defaultConnectionCallback}
    , message_callback_ {defaultMessageCallback}
    , retry_(false)
    , willing_to_connect_ {true}
    , next_conn_id_(1)
    , server_addr_ {serverAddr}
{
    // FIXME setConnectFailedCallback
    LOG_TRACE_FMT(log, "TcpClient::TcpClient[{}] - connector {}", name_, std::bit_cast<uint64_t>(connector_.get()));
}

TcpClient::~TcpClient()
{
    LOG_TRACE_FMT(log, "TcpClient::~TcpClient[{}]", name_);
    willing_to_connect_ = false;
}

void TcpClient::connect()
{
    willing_to_connect_ = true;
    // FIXME: check state
    if (connector_ == nullptr)
    {
        connector_ = std::make_unique<Connector>(loop_, server_addr_, weak_from_this());
        connector_->setNewConnectionCallback([weak_this = weak_from_this()](int sockfd) {
            if (auto server = weak_this.lock(); server != nullptr)
            {
                server->initTcpConnection_(sockfd);
            }
        });
        LOG_INFO_FMT(log, "TcpClient::connect[{}] - connecting to {}", name_, connector_->getServerAddress().toIpPortRepr());
    }
    connector_->start();
}

void TcpClient::disconnect()
{

    willing_to_connect_ = false;
    {
        auto _ = std::lock_guard<std::mutex> {mutex_};
        if (connection_ != nullptr)
        {
            connection_->shutdown();
        }
    }
}

void TcpClient::stop()
{
    willing_to_connect_ = false;
    connector_->stop();
}

void TcpClient::initTcpConnection_(int sockfd)
{
    loop_->assertInOwnerThread();
    auto peer_addr  = InetAddress {Sock::getPeerAddr(sockfd)};
    auto buf        = std::array<char, 256> {};
    auto stop_pos   = std::format_to_n(buf.begin(), buf.size(), ":{0}#{1}", peer_addr.toIpPortRepr(), next_conn_id_);
    stop_pos.out[0] = '\0';
    ++next_conn_id_;
    auto conn_name = name_ + buf.data();

    auto local_addr = InetAddress {Sock::getLocalAddr(sockfd)};
    // FIXME poll with zero timeout to double confirm the new connection
    auto conn = std::make_shared<TcpConnection>(loop_,
                                                conn_name,
                                                sockfd,
                                                local_addr,
                                                peer_addr);

    conn->setConnetionCallback(connection_callback_);
    conn->setMessageCallback(message_callback_);
    conn->setWriteCompleteCallback(write_complete_callback_);
    conn->setCloseCallback(
        [user_close_cb = conn_close_callback_, wptr = weak_from_this()](const TcpConnectionPtr& tcp_conn) {
            user_close_cb(tcp_conn);
            if (auto server = wptr.lock(); server != nullptr)
            {
                server->removeConnection_(tcp_conn);
            }
        });
    {
        auto _      = std::lock_guard<std::mutex> {mutex_};
        connection_ = conn;
    }
    conn->postConnectionCreate_();
}

void TcpClient::removeConnection_(const TcpConnectionPtr& conn)
{
    loop_->assertInOwnerThread();
    assert(loop_ == conn->getLoop());
    {
        // 没有把移除 conn 操作移至 TcpClient 所在线程
        auto _ = std::lock_guard<std::mutex> {mutex_};
        assert(connection_ == conn);
        connection_.reset();
    }

    if (retry_ && willing_to_connect_.load())
    {
        LOG_INFO_FMT(log, "TcpClient::connect[{}] - Reconnecting to {}", name_, connector_->getServerAddress().toIpPortRepr());
        connector_->restart();
    }
}