#include "logger/Logger.h"
#include "logger/LoggerManager.h"
#include "net/Callbacks.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpServer.h"
#include "common/alias.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

static auto log = GET_ROOT_LOGGER();

static const char* g_file = nullptr;
auto readFile(std::string_view filename)
    -> std::string
{
    auto fs = std::ifstream(filename.data(), std::ios::in | std::ios::binary);
    if (not fs)
    {
        LOG_ERROR_FMT(log, "open {} failed", filename);
        return "error here";
    }

    fs.seekg(0, std::ios::end);
    auto file_size = std::streamsize {fs.tellg()};
    LOG_INFO_FMT(log, "file size:{}", file_size);
    fs.seekg(0, std::ios::beg);
    auto res = std::string {};
    if (file_size > 0)
    {
        res.reserve(file_size);
        res.append(std::istreambuf_iterator<char>(fs), std::istreambuf_iterator<char>());
        // fs.read(res.data(), res.size());
        LOG_INFO_FMT(log, "res size:{}", res.size());
    }
    return res;
}

void onHighWaterMark(const TcpConnectionPtr& conn, size_t n)
{
    LOG_INFO_FMT(log, "HighWaterMark {}", n);
}

void onConnection(const TcpConnectionPtr& conn)
{
    LOG_INFO_FMT(log, "FileServer - {} -> {} is {}", conn->getPeerAddress().toIpPortRepr(), conn->getLocalAddress().toIpPortRepr(), conn->isConnected() ? "UP" : "DOWN");
    if (conn->isConnected())
    {
        LOG_INFO_FMT(log, "FileServer -SendingFile {} to {}", g_file, conn->getPeerAddress().toIpPortRepr());
        conn->setHighWaterMarkCallback(onHighWaterMark, 64 * 1_kb);
        auto file_content = readFile(g_file);
        conn->send(file_content);
        conn->shutdown();
        LOG_INFO_FMT(log, "FileServer - done");
    }
}

void onConnectionV2(const TcpConnectionPtr& conn)
{

    LOG_INFO_FMT(log, "FileServer - {} -> {} is {}", conn->getPeerAddress().toIpPortRepr(), conn->getLocalAddress().toIpPortRepr(), conn->isConnected() ? "UP" : "DOWN");
    if (conn->isConnected())
    {
        LOG_INFO_FMT(log, "FileServer -SendingFile {} to {}", g_file, conn->getPeerAddress().toIpPortRepr());
        conn->setHighWaterMarkCallback(onHighWaterMark, 64 * 1_kb);
        auto fs = std::make_shared<std::ifstream>(g_file, std::ios::in | std::ios::binary);
        if (*fs)
        {
            conn->setContext(fs);
            char rd_buf[64_kb];
            fs->read(rd_buf, sizeof(rd_buf));
            auto n_read = fs->gcount();
            if (n_read > 0)
            {
                conn->send(std::string_view {rd_buf, static_cast<size_t>(n_read)});
            }
            conn->shutdown();
            return;
        }
        else
        {
            conn->shutdown();
            LOG_ERROR_FMT(log, "open {} failed", g_file);
        }
    }
}

void onWriteCompleteV2(const TcpConnectionPtr& conn)
{
    auto fs = std::any_cast<Sptr<std::ifstream>>(conn->getContext());
    char rd_buf[64_kb];
    fs->read(rd_buf, sizeof(rd_buf));
    auto n_read = fs->gcount();
    if (n_read > 0)
    {
        conn->send(std::string_view {rd_buf, static_cast<size_t>(n_read)});
    }
    else
    {
        conn->shutdown();
        LOG_INFO_FMT(log, "FileServer - done");
    }
}

auto main(int argc, char* argv[])
    -> int
{
    LOG_INFO_FMT(log, "pid = {}", getpid());
    if (argc > 1)
    {
        g_file      = argv[1];
        auto loop   = EventLoop {};
        auto server = Sptr<TcpServer> {new TcpServer {&loop, InetAddress {2021, true}, "FileServer"}};
        if (argc == 2)
        {
            server->setConnectionEstablishedCallback(onConnection);
        }
        else if (argc > 2)
        {
            auto val = std::string {argv[2]};
            if (val == "1")
            {
                server->setConnectionEstablishedCallback(onConnection);
            }
            else if (val == "2")
            {
                server->setConnectionEstablishedCallback(onConnectionV2);
            }
            else if (val == "3")
            {
            }
            else
            {
                std::cerr << "Usage: " << argv[0] << " file_for_downloading [1 | 2 | 3]\n";
            }
        }
        server->start();
        loop.loop();
    }
    else
    {
        std::cerr << "Usage: " << argv[0] << " file_for_downloading [1 | 2 | 3]\n";
    }
    return 0;
}