#include "chargen/ChargenServer.h"
#include "daytime/DaytimeServer.h"
#include "discard/DiscardServer.h"
#include "echo/EchoServer.h"
#include "net/EventLoop.h"
#include "time/server/TimeServer.h"

template <typename... Server>
void startAll(auto&... server)  
{
    (server.start(), ...);
}
auto main()
    -> int
{
    auto loop   = EventLoop {};
    auto server = ChargenServer {&loop, InetAddress {2019, true}, true};

    auto daytime_server = DaytimeServer(&loop,
                                        InetAddress {2013, true});

    auto discard_server = DiscardServer(&loop, InetAddress(2009, true));

    auto echo_server = EchoServer(&loop, InetAddress {2007, true});

    auto time_server = TimeServer(&loop, InetAddress{2037, true});

    startAll(server, daytime_server, discard_server, echo_server, time_server);
    loop.loop();
}
