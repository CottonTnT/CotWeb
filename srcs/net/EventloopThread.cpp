#include "net/EventLoopThread.h"
#include "net/EventLoop.h"
#include <atomic>



auto EventLoopThread::getDefaultName_()
    -> std::string
{
    static auto s_thread_count = std::atomic<int>{0};
    constexpr auto c_base_name = std::string_view{"EventLoopThread-"};
    return std::string{c_base_name} + std::to_string(s_thread_count.fetch_add(1, std::memory_order_relaxed));
}

EventLoopThread::EventLoopThread()
    : loop_{ nullptr }
    , exiting_{ false }
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
    }
}

auto EventLoopThread::startLoop()
    -> EventLoop *
{
    thread_ = NamedJThread{getDefaultName_(),[this]() {
        this->threadFunc_();
    }};
    auto st = thread_.getStopToken();

    EventLoop *loop = nullptr;
    {
        while (!st.stop_requested())
        {
            // busy-waiting
        }
        loop = loop_;
    }
    return loop;
}

// 下面这个方法 是在单独的新线程里运行的
void EventLoopThread::threadFunc_()
{
    EventLoop loop; // 1.在新线程的栈上创建 EventLoop 对象
    loop_ = &loop;
    thread_.getStopSource().request_stop();
        // 通知 startLoop() 方法 可以返回了
    loop.loop();
    // 4. 执行EventLoop的loop() 开启了底层的Poller的poll()
    
    // auto lock = std::unique_lock<std::mutex>(mutex_); // 5.退出loop后, 清理 loop_
    loop_ = nullptr;
}