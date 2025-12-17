#include "ChargenServer.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"
#include "net/Callbacks.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpServer.h"
#include "net/Timestamp.h"
#include <numeric>
#include <ranges>

static auto log = GET_ROOT_LOGGER();
ChargenServer::ChargenServer(EventLoop* loop,
                             const InetAddress& listenAddr,
                             bool print)
    : server_ {new TcpServer {loop, listenAddr, "ChargenServer"}}
    , transferred_{0}
    , start_time_(Timestamp::now())
{
    server_->setConnectionEstablishedCallback([this](const auto& conn) {
        this->onConnection_(conn);
    });
    server_->setMessageCallback([this](const auto& conn,
                                       auto& buf,
                                       auto ts) {
        this->onMessage_(conn, buf, ts);
    });
    server_->setWriteCompleteCallback([this](const auto& conn) {
        this->onWriteComplete_(conn);
    });
    if (print)
    {
        loop->runEvery(3.0, [this]() {
            this->printThroughout_();
        });
    }
    auto line = std::string {};
    for (auto i : std::views::iota(33, 127))
    {
        line.push_back(static_cast<char>(i));
    }
    line += line;

    for (auto i : std::views::iota(0, 127 - 33))
    {
        msg_ += line.substr(i, 72) + '\n';
    }
}

void ChargenServer::start()
{
    server_->start();
}

void ChargenServer::onConnection_(const TcpConnectionPtr& conn)
{
    LOG_INFO_FMT(log, "ChargenServer - {} -> {} is {}", conn->getLocalAddress().toIpPortRepr(), conn->getPeerAddress().toIpPortRepr(), (conn->isConnected() ? "UP" : "DOWN"));
    if (conn->isConnected())
    {
        conn->setTcpNoDelay(true);
        conn->send(msg_);
    }
}

void ChargenServer::onMessage_(const TcpConnectionPtr& conn,
                               Buffer& buf,
                               Timestamp time)
{
    auto msg = std::string {buf.readAllAsString()};
    LOG_INFO_FMT(log, "{} discards {} bytes received at {}", conn->getName(), msg.size(), time.toString());
}

void ChargenServer::onWriteComplete_(const TcpConnectionPtr& conn)
{
    transferred_ += msg_.size();
    conn->send(msg_);
}

void ChargenServer::printThroughout_()
{
    auto ts     = Timestamp::now();
    double time = timeDifference(ts, start_time_);
    printf("%4.3f MiB/s\n", static_cast<double>(transferred_) / time / 1024 / 1024);
    transferred_ = 0;
    start_time_   = ts;
}