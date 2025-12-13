#pragma once

#include "net/EventLoopThread.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>


class EventLoop;
class EventLoopThread;

class EventLoopThreadPool 
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, std::string nameArg);
    ~EventLoopThreadPool() = default;// Don't delete loop, it's stack variable

    EventLoopThreadPool(const EventLoopThreadPool &) = delete;
    auto operator=(const EventLoopThreadPool &) -> EventLoopThreadPool & = delete;
    EventLoopThreadPool(EventLoopThreadPool &&) = delete;
    auto operator=(EventLoopThreadPool &&) -> EventLoopThreadPool & = delete;

    void setThreadNum(int numThreads) { num_threads_ = numThreads; }

    // void start(ThreadInitCallback cb = nullptr);
    void start();

    // 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
    auto getNextLoop()
        -> EventLoop *;

    auto getAllLoops()
        -> std::vector<EventLoop *>;

    [[nodiscard]] auto Started() const
        -> bool { return started_; }
    [[nodiscard]] auto getName() const
        -> const std::string&;

private:
    EventLoop *base_loop_; // 用户使用muduo创建的loop 如果线程数为1 那直接使用用户创建的loop 否则创建多EventLoop
    std::string name_;
    bool started_;
    int num_threads_;
    int next_; // 轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> sub_loops_;
};