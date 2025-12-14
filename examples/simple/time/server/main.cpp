#include "TimeServer.h"

#include "logger/LogLevel.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"

static auto log = GET_ROOT_LOGGER();
auto main()
    -> int
{
    LOG<LogLevel::INFO>(log, "pid = {}", getpid());
    auto loop = EventLoop {};
    auto listenAddr = InetAddress{2037, true};
    auto server = TimeServer(&loop, listenAddr);
    server.start();
    loop.loop();
    return 0;
}
