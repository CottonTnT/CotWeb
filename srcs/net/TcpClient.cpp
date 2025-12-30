// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

// #include "muduo/base/Logging.h"
#include "common/util.hpp"
#include "net/Callbacks.h"
#include "net/Connector.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/Socketsops.h"
#include "net/TcpConnection.h"
#include "net/TcpClient.h"
#include "logger/EasyLog.h"

#include <bit>
#include <latch>
#include <memory>

TcpClient::TcpClient(EventLoop* loop,
                     const InetAddress& serverAddr,
                     std::string nameArg)
    : loop_ {UtilT::requiresNonNull(loop)}
    , connector_ {new Connector {loop, serverAddr}}
    , name_ {std::move(nameArg)}
    , conn_established_callback_ {defaultConnectionCallback}
    , conn_close_callback_ {defaultConnectionCallback}
    , message_callback_ {defaultMessageCallback}
    , keep_retry_ {false}
    , willing_to_connect_ {true}
    , next_conn_id_(1)
    , server_addr_ {serverAddr}
{
    // FIXME setConnectFailedCallback
    EASY_DEBUG("TcpClient::TcpClient[{}] - connector {}", name_, std::bit_cast<uint64_t>(connector_.get()));
    connector_->setNewConnectionCallback([this](int sockfd) {
        this->initTcpConnection_(sockfd);
    });
}

TcpClient::~TcpClient()
{
    loop_->assertInOwnerThread();
    EASY_DEBUG("TcpClient::~TcpClient[{}]", name_);
    auto latch = std::latch {1};
    loop_->runInLoop([this, &latch]() {
        if (connection_ != nullptr)
        {
            auto guard = connection_;
            // 把原始的智能指针复位 让栈空间的TcpConnectionPtr conn指向该对象 当conn出了其作用域 即可释放智能指针指向的对象
            // 销毁连接
            connection_.reset();
            loop_->runInLoop([guard] {
                guard->forceCloseInOwnerLoop_();
            });
        }
        latch.count_down();
    });
    latch.wait();
}

void TcpClient::connect()
{
    TRACE();
    willing_to_connect_ = true;
    EASY_INFO("TcpClient::connect[{}] - connecting to {}", this->name_, this->connector_->getServerAddress().toIpPortRepr());
    connector_->start();
}

void TcpClient::disconnect()
{
    TRACE();
    willing_to_connect_ = false;
    loop_->runInLoop([weak_guard = weak_from_this()]() {
        if (auto guard = weak_guard.lock(); guard != nullptr)
        {
            if (guard->connection_ != nullptr)
            {
                guard->connection_->shutdown();
            }
        }
        else
        {
            EASY_WARN("TcpClient::disconnect - client destruct already");
        }
    });
}

void TcpClient::stop()
{
    TRACE();
    willing_to_connect_ = false;
    this->connector_->stop();
}

void TcpClient::initTcpConnection_(int sockfd)
{
    TRACE();
    loop_->assertInOwnerThread();
    auto peer_addr  = InetAddress {Sock::getPeerAddr(sockfd)};
    auto buf        = std::array<char, 256> {};
    auto stop_pos   = std::format_to_n(buf.begin(), buf.size(), ":{0}#{1}", peer_addr.toIpPortRepr(), next_conn_id_);
    stop_pos.out[0] = '\0';
    ++next_conn_id_;
    auto conn_name = name_ + buf.data();

    auto local_addr = InetAddress {Sock::getLocalAddr(sockfd)};
    // FIXME poll with zero timeout to double confirm the new connection
    auto conn = TcpConnection::create(loop_,
                                                conn_name,
                                                sockfd,
                                                local_addr,
                                                peer_addr);

    conn->setConnetionCallback(conn_established_callback_);
    conn->setMessageCallback(message_callback_);
    conn->setWriteCompleteCallback(write_complete_callback_);
    conn->setCloseCallback(
        [user_close_cb = conn_close_callback_, wptr = weak_from_this()](const TcpConnectionPtr& tcp_conn) {
            user_close_cb(tcp_conn);
            if (auto server = wptr.lock(); server != nullptr)
            {
                server->removeConnInLoop_(tcp_conn);
            }
        });
    assert(connection_ == nullptr);
    connection_ = conn;
    loop_->runInLoop([conn]() {
        conn->initInLoop_();
    });
}

void TcpClient::removeConnInLoop_(const TcpConnectionPtr& conn)
{
    TRACE();
    loop_->assertInOwnerThread();
    assert(connection_ == conn);
    connection_.reset();
    if (keep_retry_ && willing_to_connect_)
    {
        EASY_INFO("TcpClient::connect[{}] - Reconnecting to {}", name_, connector_->getServerAddress().toIpPortRepr());
        connector_->restart();
    }
}