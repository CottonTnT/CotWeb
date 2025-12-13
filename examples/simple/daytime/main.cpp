#include "DaytimeServer.h"
#include "net/EventLoop.h"

#include <unistd.h>

auto main() -> int
{
    std::println("pid={}", ::getpid());
    EventLoop loop;
    InetAddress listen_addr(2013, true);
    DaytimeServer server(&loop, listen_addr);
    server.start();
    loop.loop();
}