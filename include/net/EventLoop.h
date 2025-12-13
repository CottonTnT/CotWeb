#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#include "Timestamp.h"
#include "common/curthread.h"
#include "net/Callbacks.h"
#include "net/TimerQueue.h"

class Channel;
class EPollPoller;

// 事件循环类 主要包含了两个大模块 Channel Poller(epoll的抽象)
/**
 * @brief Reactor, at most one per thread
 * @attention 1. IO thread i.e. the thread that creates the EventLoop object
2. the life time of EventLoop object is same as the thread so that it no need to be a heap object
 */
class EventLoop {

public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Task      = std::function<void()>;
    using ChannelList = std::vector<Channel*>;
private:
    /**
     * @brief 是否正在事件循环中
     */
    std::atomic<bool> looping_;

    /**
     * @brief 标识退出loop循环
     */
    std::atomic<bool> quit_;

    /**
     * @brief 标识当前EventLoop是否有事件正在处理
     */
    std::atomic<bool> event_handling_;

    /**
     * @brief 标识当前loop是否有需要执行的回调操作
     */
    std::atomic<bool> calling_pending_tasks_;

    uint64_t iteration_count_;

    /**
     * @brief 记录当前 EventLoop 是被哪个线程id创建的 即标识了当前EventLoop的所属线程id
     */
    std::jthread::id owner_tid_;

    /**
     * @brief Poller返回发生事件的Channels的时间点
     */
    Timestamp last_poll_return_time_;
    // TimePoint last_poll_return_time_;

    std::unique_ptr<EPollPoller> poller_;
    std::unique_ptr<TimerQueue> timer_queue_;

    /**
     * @brief 作用：当mainLoop获取一个新用户的Channel 需通过轮询算法选择一个subLoop, 通过 subLoop 的 wake_up_channel_唤醒该subLoop
     */
    int wakeup_fd_;

    std::unique_ptr<Channel> wakeup_channel_;

    /**
     * @brief 返回Poller检测到当前有事件发生的所有 Channel 列表
     */
    ChannelList active_channels_;

    /**
     * @brief 存储loop需要执行的所有回调操作
     */
    std::vector<Task> pending_tasks_;
    std::mutex mutex_;

    /**
     * @brief 通过eventfd唤醒loop所在的线程
     */
    void wakeupOwnerThread_() const;

    void wakeChannelReadCallback_() const; // 给eventfd返回的文件描述符wakeupFd_绑定的事件回调 当wakeup()时 即有事件发生时 调用handleRead()读wakeupFd_的8字节 同时唤醒阻塞的epoll_wait
    void runPendingTasks_();               // 执行上层回调

    void AbortNotInLoopThread_() const;

public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&)                    = delete;
    auto operator=(const EventLoop&) -> EventLoop& = delete;
    EventLoop(EventLoop&&)                         = delete;
    auto operator=(EventLoop&&) -> EventLoop&      = delete;

    /**
     * @brief 开启事件循环
     * @attention must be called in the IO thread that creates the EventLoop object
     */
    void loop(std::error_code& ec);
    void loop();
    // 退出事件循环

    /**
     * @brief Quits loop
     * This is not 100% thread safe, if you call through a raw pointer,
     * better to call through shared_ptr<EventLoop> for 100% safety.
     */
    void quit(std::error_code& ec);
    void quit();

    [[nodiscard]] auto getLastPollReturnTime() const
        -> TimePoint { return last_poll_return_time_.toTimePoint(); }

    /**
     * @brief Runs callback immediately in the loop thread.
     * It wakes up the loop, and run the cb.
     * If in the same loop thread, cb is run within the function.
     * Safe to call from other threads.
     */
    void runTask(Task task);

    /**
     * @brief 把上层注册的回调函数cb放入队列中 唤醒loop所在的线程执行cb
     * Queues callback in the loop thread.
     * Runs after finish pooling.
     * Safe to call from other threads.
     */
    void queueTask(Task task);

    // EventLoop的方法 => Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    auto hasChannel(Channel* channel)
        -> bool;

    void assertInOwnerThread() const
    {
        if (not inOwnerThread())
        {
            // todo: log and exit
            std::terminate();
        }
    }

    // 判断EventLoop对象是否在自己的线程里
    [[nodiscard]] auto inOwnerThread() const
        -> bool { return owner_tid_ == CurThr::GetId(); } // threadId_为EventLoop创建时的线程id CurrentThread::tid()为当前线程id

    /* ======================== timers ======================== */

    ///
    /// Runs callback at 'time'.
    /// Safe to call from other threads.
    ///
    auto runAt(Timestamp time, TimerCallback cb)
        -> Timer::Id;

    ///
    /// Runs callback after @c delay seconds.
    /// Safe to call from other threads.
    ///
    auto runAfter(double delay_seconds, TimerCallback cb)
        -> Timer::Id;

    ///
    /// Runs callback every @c interval seconds.
    /// Safe to call from other threads.
    ///
    auto runEvery(double interval, TimerCallback cb)
        -> Timer::Id;
    ///
    /// Cancels the timer.
    /// Safe to call from other threads.
    ///
    void cancelTimer(Timer::Id timerId);

    static auto getEventLoopOfCurrentThread() -> EventLoop*;
};