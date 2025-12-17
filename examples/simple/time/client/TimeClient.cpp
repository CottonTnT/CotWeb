#include "logger/Logger.h"
#include "logger/LoggerManager.h"
#include "net/Callbacks.h"
#include "net/Endian.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpClient.h"
#include "net/Timestamp.h"

#include <memory>
#include <utility>

#include <stdio.h>
#include <unistd.h>

static auto log = GET_ROOT_LOGGER();
class TimeClient {
public:
    TimeClient(EventLoop* loop, const InetAddress& serverAddr)
        : loop_(loop)
        , client_ {new TcpClient {loop, serverAddr, "TimeClient"}}
    {
        client_->setConnetionCallback([this](const auto& conn) {
            this->onConnection(conn);
        });
        client_->setConnectionCloseCallback([this](const auto& conn) {
            this->onConnection(conn);
        });
        client_->setMessageCallback([this](const auto& conn, auto& buf, auto ts) {
            this->onMessage(conn, buf, ts);
        });
        // client_.enableRetry();
    }

    void connect()
    {
        client_->connect();
    }

private:
    EventLoop* loop_;
    std::shared_ptr<TcpClient> client_;

    void onConnection(const TcpConnectionPtr& conn)
    {

        LOG_DEBUG_FMT(log, "TimeClient - {} ->{}  is  {}", conn->getLocalAddress().toIpPortRepr(), conn->getLocalAddress().toIpPortRepr(), conn->isConnected() ? "UP" : "DOWN");

        if (not conn->isConnected())
        {
            loop_->quit();
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer& buf, Timestamp receiveTime)
    {
        if (buf.getReadableBytesCount() >= sizeof(int32_t))
        {
            time_t data   = buf.readInteger<uint32_t>();
            auto a      = Timestamp {static_cast<uint64_t>(data) * Timestamp::c_micro_seconds_per_second};
            LOG_INFO_FMT(log, "\n Server time = {}, {}", data, a.toFormattedString());
        }
        else
        {
            LOG_INFO_FMT(log, "{} no enough data {} at {}", conn->getName(), buf.getReadableBytesCount(), receiveTime.toFormattedString());
        }
    }
};

// auto main(int argc, char* argv[])
//     -> int
// {
//     LOG_INFO_FMT(log, "pid = {}", getpid());
//     if (argc > 1)
//     {
//         EventLoop loop;
//         InetAddress serverAddr(argv[1], 2037);

//         TimeClient timeClient(&loop, serverAddr);
//         timeClient.connect();
//         loop.loop();
//     }
//     else
//     {
//         printf("Usage: %s host_ip\n", argv[0]);
//     }
// }