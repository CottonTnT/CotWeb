#pragma once

#include "common/NamedJThread.h"
#include <functional>
#include <string>

class EventLoop;

class EventLoopThread
{
public:
    explicit EventLoopThread();
    ~EventLoopThread();
    EventLoopThread(const EventLoopThread &) = delete;
    auto operator=(const EventLoopThread &) -> EventLoopThread & = delete;
    EventLoopThread(EventLoopThread &&) = delete;
    auto operator=(EventLoopThread &&) -> EventLoopThread & = delete;

    auto startLoop()
        -> EventLoop *;

private:

    static auto getDefaultName_()
        -> std::string;
    void threadFunc_();
    EventLoop *loop_;
    bool exiting_;
    NamedJThread thread_;
};