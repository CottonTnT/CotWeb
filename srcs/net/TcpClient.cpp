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

// TcpClient::TcpClient(EventLoop* loop) //   : loop_(loop)
// {
// }

// TcpClient::TcpClient(EventLoop* loop, const string& host, uint16_t port)
//   : loop_(CHECK_NOTNULL(loop)),
//     serverAddr_(host, port)
// {
// }
static auto log = GET_ROOT_LOGGER();
namespace detail {

static void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn)
{
    // loop->queueTask([conn] { conn->con(); });
}

void removeConnector(const ConnectorPtr& connector)
{
    // connector->
}
}

TcpClient::TcpClient(EventLoop* loop,
                     const InetAddress& serverAddr,
                     std::string nameArg)
    : loop_ {loop}
    , connector_ {new Connector {loop, serverAddr}}
    , name_ {std::move(nameArg)}
    , connection_callback_ {defaultConnectionCallback}
    , conn_close_callback_ {defaultConnectionCallback}
    , message_callback_ {defaultMessageCallback}
    , retry_(false)
    , already_start_connect_ {false}
    // , keep_connection_(true)
    , next_conn_id_(1)
{
    connector_->setNewConnectionCallback([this](int sockfd) {
        this->initTcpConnection_(sockfd);
    });
    // FIXME setConnectFailedCallback
    LOG_TRACE_FMT(log, "TcpClient::TcpClient[{}] - connector {}", name_, std::bit_cast<uint64_t>(connector_.get()));
}

TcpClient::~TcpClient()
{
    // connection_->destroyConnection_();
    // connection_.reset();
    // LOG<LogLevel::INFO>(log, "TcpClient::TcpClient[{}] - connector {}", name_, std::bit_cast<uint64_t>(connector_.get()));
    // TcpConnectionPtr conn;
    // bool unique = false;
    // {
    //     auto _ = std::lock_guard<std::mutex> {mutex_};
    //     unique = connection_.unique();
    //     conn   = connection_;
    // }
    // if (conn)
    // {
    //     assert(loop_ == conn->getLoop());
    //     // FIXME: not 100% safe, if we are in different thread
    //     auto close_cb = [this](const TcpConnectionPtr& tcp_conn) {
    //         this->loop_->queueTask([tcp_conn]() {

    //         });
    //     };
    //     // CloseCallback cb = std::bind(&detail::removeConnection, loop_, std::placeholders::_1);
    //     loop_->runTask(
    //         [conn, close_cb] { conn->setCloseCallback(close_cb); });
    //     if (unique)
    //     {
    //         conn->forceClose();
    //     }
    // }
    // else
    // {
    //     connector_->stop();
    //     // FIXME: HACK
    //     loop_->runAfter(1, [this] { detail::removeConnector(this->connector_); });
    // }
}

void TcpClient::connect()
{
    if (not already_start_connect_.exchange(true))
    {
        // FIXME: check state
        LOG_INFO_FMT(log, "TcpClient::connect[{}] - connecting to {}", name_, connector_->getServerAddress().toIpPortRepr());
        connector_->start();
    }
}

void TcpClient::disconnect()
{

    if (already_start_connect_.exchange(false))
    {
        {
            auto _ = std::lock_guard<std::mutex> {mutex_};
            if (connection_ != nullptr)
            {
                connection_->shutdown();
            }
        }
    }
}

void TcpClient::stop()
{
    if (already_start_connect_.exchange(false))
    {
        connector_->stop();
    }
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
    // FIXME use make_shared if necessary
    auto conn = std::make_shared<TcpConnection>(loop_,
                                                conn_name,
                                                sockfd,
                                                local_addr,
                                                peer_addr);

    conn->setConnetionCallback(connection_callback_);
    conn->setMessageCallback(message_callback_);
    conn->setWriteCompleteCallback(write_complete_callback_);
    conn->setCloseCallback(
        [user_close_cb = conn_close_callback_, wptr = std::weak_ptr<TcpClient> {shared_from_this()}](const TcpConnectionPtr& tcp_conn) {
            user_close_cb(tcp_conn);
            if (auto guard = wptr.lock(); guard != nullptr)
            {
                guard->removeConnection_(tcp_conn);
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
        auto _ = std::lock_guard<std::mutex> {mutex_};
        assert(connection_ == conn);
        connection_.reset();
    }

    if (retry_ && already_start_connect_.load())
    {
        LOG_INFO_FMT(log, "TcpClient::connect[{}] - Reconnecting to {}", name_, connector_->getServerAddress().toIpPortRepr());
        connector_->restart();
    }
}