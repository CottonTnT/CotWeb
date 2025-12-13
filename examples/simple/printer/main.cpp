#include "net/EventLoop.h"
#include "net/TimerQueue.h"
#include <print>
#include <thread>


auto main() -> int
{
    auto loop = EventLoop{};
    loop.runAfter(5, []() {
        std::println("Timer fired after 1 second!");
    });
    auto id1 = loop.runEvery(2, []() {
        std::println("Timer fired every 2 seconds!");
    });

    auto id2 = loop.runEvery(4, []() {
        std::println("Timer fired every 4 seconds!");
    });
    auto hle =std::jthread([&loop, id1, id2]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::println("Canceling timer...");
        loop.cancelTimer(id1);
        loop.cancelTimer(id2);
    });
    hle.detach();
    loop.loop();
    return 0;
}