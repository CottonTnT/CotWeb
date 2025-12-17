#include "EchoServer.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"

static auto log = GET_ROOT_LOGGER();
auto main()
    -> int
{
    LOG_INFO_FMT(log, "pid = ", getpid());
    auto loop   = EventLoop();
    auto server = EchoServer(&loop, InetAddress {2007, true});
    server.start();
    loop.loop();
    return 0;
}