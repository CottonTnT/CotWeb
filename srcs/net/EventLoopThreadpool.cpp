#include <algorithm>
#include <format>
#include <memory>
#include <ranges>

#include "net/EventLoopThreadpool.h"
#include "net/EventLoopThread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop,  std::string nameArg)
    : base_loop_{ baseLoop }
    , name_{ std::move(nameArg) }
    , started_{ false }
    , num_threads_{ 0 }
    , next_{ 0 }
{
}

void EventLoopThreadPool::start()
{
    started_ = true;

    for(auto i : std::views::iota(0, num_threads_))
    {
        // auto thread_name = std::string(name_.size() + 33, '\0'); std::format_to_n(thread_name.data(), thread_name.size(), "{}#{}", name_, i);
        auto *t = new EventLoopThread{};
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        // 底层创建线程 绑定一个新的EventLoop 并返回该loop的地址
        sub_loops_.push_back(t->startLoop());
    }

    // if(num_threads_ == 0 && cb)                                      // 整个服务端只有一个线程运行baseLoop
    // {
    //     cb(base_loop_);
    // }
}

// 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
auto EventLoopThreadPool::getNextLoop()
    -> EventLoop *
{
    // 如果只设置一个线程 也就是只有一个mainReactor 无subReactor 那么轮询只有一个线程 getNextLoop()每次都返回当前的baseLoop_
    auto *loop = base_loop_;    

    // 通过轮询获取下一个处理事件的loop
    if(not sub_loops_.empty())             
    {
        loop = sub_loops_[next_];
        ++next_;
        if(next_ >= sub_loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}

auto EventLoopThreadPool::getAllLoops()
    -> std::vector<EventLoop *>
{
    if(sub_loops_.empty())
    {
        return std::vector<EventLoop *>(1, base_loop_);
    }
    return sub_loops_;
}
auto EventLoopThreadPool::getName() const
    -> const std::string& { return name_; }
