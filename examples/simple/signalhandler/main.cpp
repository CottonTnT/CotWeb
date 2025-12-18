#include <csignal>
#include <net/TcpServer.h>
#include <net/EventLoop.h>
#include "logger/EasyLog.h"
#include "net/SignalHandler.h"
#include <iostream>


auto main()
    -> int {
    EventLoop loop;
    // InetAddress addr(8888);
    // TcpServer server(&loop, addr, "EchoServer");

    // server.start();

    SignalHandler signalHandler(&loop);

    signalHandler.addSignal(SIGINT);
    signalHandler.addSignal(SIGTERM);
    signalHandler.addSignal(SIGHUP);
    signalHandler.addSignal(SIGQUIT);

    signalHandler.setCallback([&](int signo) {
        switch (signo) {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            EASY_DEBUG("Graceful shutdown");
            loop.quit();
            break;

        case SIGHUP:
            std::cout << "Reload config/log";
            break;

        default:
            break;
        }
    });

    signalHandler.start();
    loop.loop();
}
