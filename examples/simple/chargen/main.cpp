#include "logger/Logger.h"
#include "logger/LoggerManager.h"
#include "ChargenServer.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"

auto main(int argc, char** argv)
    -> int
{
    auto loop = EventLoop();
    auto server = ChargenServer{&loop, InetAddress{2019 , true}, true};
    server.start();
    loop.loop();
    return 0;
}