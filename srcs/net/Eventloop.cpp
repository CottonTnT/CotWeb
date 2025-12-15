#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <exception>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <sys/eventfd.h>
#include <system_error>
#include <unistd.h>
#include <signal.h>

#include "net/EventLoop.h"
#include "common/curthread.h"
#include "logger/LogLevel.h"
#include "net/EventLoopErrc.h"
#include "net/Channel.h"
#include "net/Epoller.h"
#include "net/Timestamp.h"
#include "logger/Logger.h"
#include "logger/LoggerManager.h"


static auto log = GET_ROOT_LOGGER();
namespace {
auto createEventfd()
    -> int
{
    auto evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        // LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

// 防止一个线程创建多个EventLoop
thread_local EventLoop* t_event_loop = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000; // 10000毫秒 = 10秒钟

class IgnoreSigPipe {
public:
    IgnoreSigPipe()
    {
        std::ignore = ::signal(SIGPIPE, SIG_IGN);
        // todo:log
        //  LOG_TRACE << "Ignore SIGPIPE";
    }
};

IgnoreSigPipe initObj;

} // namespace

/* 创建线程之后主线程和子线程谁先运行是不确定的。
 * 通过一个eventfd在线程之间传递数据的好处是多个线程无需上锁就可以实现同步。
 * eventfd支持的最低内核版本为Linux 2.6.27,在2.6.26及之前的版本也可以使用eventfd，但是flags必须设置为0。
 * 函数原型：
 *     #include <sys/eventfd.h>
 *     int eventfd(unsigned int initval, int flags);
 * 参数说明：
 *      initval,初始化计数器的值。
 *      flags, EFD_NONBLOCK,设置socket为非阻塞。
 *             EFD_CLOEXEC，执行fork的时候，在父进程中的描述符会自动关闭，子进程中的描述符保留。
 * 场景：
 *     eventfd可以用于同一个进程之中的线程之间的通信。
 *     eventfd还可以用于同亲缘关系的进程之间的通信。
 *     eventfd用于不同亲缘关系的进程之间通信的话需要把eventfd放在几个进程共享的共享内存中（没有测试过）。
 */
// 创建wakeupfd 用来notify唤醒subReactor处理新来的channel

EventLoop::EventLoop()
    : looping_ {false}
    , quit_ {false}
    , event_handling_ {false}
    , calling_pending_tasks_ {false}
    , iteration_count_ {0}
    , owner_tid_ {CurThr::GetId()}
    , poller_ {std::make_unique<EPollPoller>(this)}
    , timer_queue_ {new TimerQueue {this}}
    , wakeup_fd_ {createEventfd()}
    , wakeup_channel_ {new Channel {this, wakeup_fd_}}
{
    LOG_DEBUG_FMT(log, "EventLoop created {} in thread {}", std::bit_cast<uint64_t>(this), owner_tid_);
    // only one loop per thread
    if (t_event_loop != nullptr)
    {
        // todo log and exit
        LOG_FATAL_FMT(log, "Another EventLoop %p exists in this thread %d", std::bit_cast<uint64_t>(t_event_loop), owner_tid_);
    }
    else
        t_event_loop = this;
    // 设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeup_channel_->setReadCallback([this](Timestamp) {
        this->wakeChannelReadCallback_();
    });

    // 每一个EventLoop都将监听wakeupChannel_的EPOLL读事件了
    wakeup_channel_->enableReading();
}

EventLoop::~EventLoop()
{
    // 给Channel移除所有感兴趣的事件
    wakeup_channel_->unregisterAllEvent();
    // 把Channel从EventLoop上删除掉
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
    t_event_loop = nullptr;
}

void EventLoop::loop()
{
    std::error_code ec;
    loop(ec);
    if (ec)
    {
        throw std::system_error {ec};
        // LOG_ERROR("EventLoop::loop() error: %s\n", ec.message().c_str());
    }
}
// 开启事件循环
void EventLoop::loop(std::error_code& ec)
{
    if (looping_.exchange(true))
    {
        ec = EventLoopErrcCode::AlreadyInLooping;
        return;
    }
    if (not inOwnerThread())
    {
        ec = EventLoopErrcCode::NotInOwnerLoop;
        return;
    }
    LOG_INFO_FMT(log,"EventLoop {} start looping", std::bit_cast<uint64_t>(this));
    while (not quit_.load())
    {
        active_channels_.clear();
        // 1. 等待事件发生
        last_poll_return_time_ = poller_->poll(kPollTimeMs, active_channels_);
        ++iteration_count_;

        // 2. 处理活跃 Channel 的事件
        std::ranges::for_each(active_channels_, [this](auto* channel) {
            // Poller监听哪些channel发生了事件 然后上报给EventLoop 通知channel处理相应的事件
            channel->handleEvent(last_poll_return_time_);
        });
        /**
         * 执行当前EventLoop事件循环需要处理的回调操作 对于线程数 >=2 的情况 IO线程 mainloop(mainReactor) 主要工作：
         * accept接收连接 => 将accept返回的connfd打包为Channel => TcpServer::newConnection通过轮询将TcpConnection对象分配给subloop处理
         *
         * mainloop调用QueueInOwnerLoop将回调加入subloop（该回调需要subloop执行 但subloop还在poller_->poll处阻塞） queueInLoop通过wakeup将subloop唤醒
         **/
        // 3. 执行 EventLoop 内部任务队列中的任务
        runPendingTasks_();
    }
    LOG_INFO_FMT(log,"EventLoop %p stop looping.\n", std::bit_cast<uint64_t>(this));
    looping_ = false;
}

/**
 * 退出事件循环
 * 1. 如果loop在自己的线程中调用quit成功了 说明当前线程已经执行完毕了loop()函数的poller_->poll并退出
 * 2. 如果不是当前EventLoop所属线程中调用quit退出EventLoop 需要唤醒EventLoop所属线程的epoll_wait
 *
 * 比如在一个subloop(worker)中调用mainloop(IO)的quit时 需要唤醒mainloop(IO)的poller_->poll 让其执行完loop()函数
 *
 * ！！！ 注意： 正常情况下 mainloop负责请求连接 将回调写入subloop中 通过生产者消费者模型即可实现线程安全的队列
 * ！！！       但是muduo通过wakeup()机制 使用eventfd创建的wakeupFd_ notify 使得mainloop和subloop之间能够进行通信
 **/
void EventLoop::quit(std::error_code& ec)
{

    if (quit_.exchange(true))
    {
        ec = EventLoopErrcCode::AlreadyQuit;
    }
    {
        if (not looping_.load())
        {
            ec = EventLoopErrcCode::NotLoopingYet;
            return;
        }
    }
    quit_ = true;
    if (not inOwnerThread())
    {
        wakeupOwnerThread_();
    }
}

void EventLoop::quit()
{
    std::error_code ec;
    quit(ec);
    if (ec)
    {
        throw std::system_error {ec};
        // LOG_ERROR("EventLoop::quit() error: %s\n", ec.message().c_str());
    }
}

// 在当前loop中执行cb
void EventLoop::runTask(Task task)
{
    if (inOwnerThread()) // 当前 EventLoop 中直接同步执行任务
    {
        task();
    }
    else
    {
        queueTask(std::move(task));
    }
}

// 把cb放入队列中 唤醒loop所在的线程执行cb
void EventLoop::queueTask(Task task)
{
    {
        auto _ = std::unique_lock<std::mutex> {mutex_};
        pending_tasks_.emplace_back(std::move(task));
    }

    /**
     * callingPendingTasks的意思是 当前loop正在执行回调中 但是loop的pendingTasks_中又加入了新的回调 需要通过 wakeup 写事件
     * 唤醒相应的需要执行上面回调操作的loop的线程 让loop()下一次poller_->poll()不再阻塞（阻塞的话会延迟前一次新加入的回调的执行），然后
     * 继续执行pendingFunctors_中的回调函数
     **/
    if (not inOwnerThread() or calling_pending_tasks_)
    {
        wakeupOwnerThread_(); // 唤醒loop所在线程
    }
}

void EventLoop::wakeChannelReadCallback_() const
{
    auto one = uint64_t {1};
    auto n   = read(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        // LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
    }
}

void EventLoop::wakeupOwnerThread_() const
{
    auto one = uint64_t {1};
    auto n   = write(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        // LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}
auto EventLoop::runAt(Timestamp time, TimerCallback cb)
    -> Timer::Id
{
    return timer_queue_->addTimer(std::move(cb), time, 0.0);
}

auto EventLoop::runAfter(double delay, TimerCallback cb)
    -> Timer::Id
{
    Timestamp time(addTime(Timestamp::Now(), delay));
    return runAt(time, std::move(cb));
}

auto EventLoop::runEvery(double interval, TimerCallback cb)
    -> Timer::Id
{
    Timestamp time(addTime(Timestamp::Now(), interval));
    return timer_queue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancelTimer(Timer::Id timerId)
{
    return timer_queue_->cancel(timerId);
}

// EventLoop的方法 => Poller的方法
void EventLoop::updateChannel(Channel* channel)
{
    assert(channel->getOwnerLoop() == this);
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}

auto EventLoop::hasChannel(Channel* channel)
    -> bool
{
    return poller_->hasChannel(channel);
}

void EventLoop::runPendingTasks_()
{
    auto tasks             = std::vector<Task> {};
    calling_pending_tasks_ = true;
    {
        auto _ = std::unique_lock<std::mutex>(mutex_);
        tasks.swap(pending_tasks_); // 交换的方式减少了锁的临界区范围 提升效率 同时避免了死锁 如果执行functor()在临界区内 且functor()中调用QueueInOwnerLoop()就会产生死锁
    }
    std::ranges::for_each(tasks, [](const auto& task) {
        task();
    });
    calling_pending_tasks_ = false;
}

void EventLoop::AbortNotInLoopThread_() const
{
    // todo
    //    LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
    //              << " was created in threadId_ = " << threadId_
    //              << ", current thread id = " <<  CurrentThread::tid();
}

auto getEventLoopOfCurrentThread()
    -> EventLoop*
{
    return t_event_loop;
}