// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

// #include "muduo/base/Logging.h"
#include "net/Callbacks.h"
#include "net/Connector.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/Socketsops.h"
#include "net/TcpConnection.h"
#include "net/TcpClient.h"


#include <memory>

// TcpClient::TcpClient(EventLoop* loop)
//   : loop_(loop)
// {
// }

// TcpClient::TcpClient(EventLoop* loop, const string& host, uint16_t port)
//   : loop_(CHECK_NOTNULL(loop)),
//     serverAddr_(host, port)
// {
// }
namespace detail {

static void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn)
{
    // loop->queueTask([tcpconn = conn] { tcpconn->connectionD(); });
}

void removeConnector(const ConnectorPtr& connector)
{
    // connector->
}
}

TcpClient::TcpClient(EventLoop* loop,
                     const InetAddress& serverAddr,
                     std::string nameArg)
    : loop_{ loop }
    , connector_{ new Connector(loop, serverAddr) }
    , name_(std::move(nameArg))
    , conn_established_callback_ {defaultConnectionCallback}
    , conn_close_callback_{ defaultConnectionCallback }
    , message_callback_{ defaultMessageCallback }
    , retry_(false)
    , not_connect_(true)
    , next_conn_id_(1)
{
    // connector_->SetNewConnectionCallback(
    //     std::bind(&TcpClient::newConnection, this, std::placeholders::_1));
    connector_->setConnectionEstablishedCB([this](int sockfd) { this->initNewConnection_(sockfd); });
    // FIXME setConnectFailedCallback
    //   LOG_INFO << "TcpClient::TcpClient[" << name_
    //            << "] - connector " << get_pointer(connector_);
}

TcpClient::~TcpClient()
{
    //   LOG_INFO << "TcpClient::~TcpClient[" << name_
    //            << "] - connector " << get_pointer(connector_);
    TcpConnectionPtr conn;
    bool unique = false;
    {
        auto _ = std::lock_guard<std::mutex> {mutex_};
        unique = connection_.unique();
        conn   = connection_;
    }
    if (conn)
    {
        assert(loop_ == conn->getLoop());
        // FIXME: not 100% safe, if we are in different thread
        auto close_cb = [event_loop = loop_](const TcpConnectionPtr& tcp_conn) {
            detail::removeConnection(event_loop, tcp_conn);
        };
        // CloseCallback cb = std::bind(&detail::removeConnection, loop_, std::placeholders::_1);
        loop_->runTask(
            [conn, close_cb] { conn->setCloseCallback(close_cb); });
        if (unique)
        {
            conn->forceClose();
        }
    }
    else
    {
        connector_->stop();
        // FIXME: HACK
        loop_->runAfter(1, std::bind(&detail::removeConnector, connector_));
    }
}

void TcpClient::connect()
{
    // FIXME: check state
    //   LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to "
    //            << connector_->serverAddress().toIpPort();
    not_connect_ = true;
    connector_->start();
}

void TcpClient::Disconnect()
{
    not_connect_ = false;

    {
        auto _ = std::lock_guard<std::mutex> {mutex_};
        if (connection_)
        {
            connection_->shutdown();
        }
    }
}

void TcpClient::Stop()
{
    not_connect_ = false;
    connector_->stop();
}

void TcpClient::initNewConnection_(int sockfd)
{
    loop_->assertInOwnerThread();
    auto peer_addr = InetAddress {Sock::GetPeerAddr(sockfd)};
    auto buf       = std::array<char, 32> {};
    auto stop_pos = std::format_to_n(buf.begin(), buf.size(), ":{0}#{1}", peer_addr.toIpPortRepr(), next_conn_id_);
    stop_pos.out[0] = '\0';
    ++next_conn_id_;
    auto conn_name = name_ + buf.data();

    auto local_addr = InetAddress{ Sock::GetLocalAddr(sockfd) };
    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    auto conn = std::make_shared<TcpConnection>( loop_,
                                            conn_name,
                                            sockfd,
                                            local_addr,
                                            peer_addr);

    conn->setConnEstablishedCB(conn_established_callback_);
    conn->setMessageCallback(message_callback_);
    conn->setWriteCompleteCallback(write_complete_callback_);
    conn->setCloseCallback(
        [tcp_client = this](const TcpConnectionPtr& tcp_conn) {
            tcp_client->conn_close_callback_(tcp_conn);
            tcp_client->RemoveConnection_(tcp_conn);
        }
    ); // FIXME: unsafe
    {
        auto _      = std::lock_guard<std::mutex> {mutex_};
        connection_ = conn;
    }
    // conn->postConnectionCreate_();
}

void TcpClient::RemoveConnection_(const TcpConnectionPtr& conn)
{
    loop_->assertInOwnerThread();
    assert(loop_ == conn->getLoop());

    {
        auto _ = std::lock_guard<std::mutex> {mutex_};
        assert(connection_ == conn);
        connection_.reset();
    }

    // loop_->queueTask([conn] { conn->ConnectDestroyedHandler(); });
    if (retry_ && not_connect_)
    {
        // todo:log
        // LOG_INFO << "TcpClient::connect[" << name_ << "] - Reconnecting to "
        //          << connector_->serverAddress().toIpPort();
        connector_->restart();
    }
}
