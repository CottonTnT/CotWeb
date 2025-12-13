#include "DiscardServer.h"
#include "net/EventLoop.h"

#include <unistd.h>

auto main()
    -> int
{
    EventLoop loop;
    InetAddress listenAddr(2009, true);
    DiscardServer server(&loop, listenAddr);
    server.start();
    loop.loop();
    return 0;
}
