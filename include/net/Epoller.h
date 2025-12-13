#pragma once

#include <chrono>
#include <sys/epoll.h>
#include <vector>

#include "Timestamp.h"
#include "EventLoop.h"
#include "common/alias.h"

/**
 * epoll的使用:
 * 1. epoll_create
 * 2. epoll_ctl (add, mod, del)
 * 3. epoll_wait
 **/

class Channel;

/**
 * @tag inner implementation class for EventLoop, user invisible
 * @brief IO复用类 epoll的封装, only responsible for IO multiplexing, not dispatching the events
 * @attention
 * 1. one Poller only belongs to one EventLoop, one EventLoop only belongs to one thread
 * 2. poller does not own the channels that the channels must unregister itself from poller before it destructs
 */
class EPollPoller 
{
public:
    using ChannelList = std::vector<Channel *>;

    EPollPoller(const EPollPoller&)            = delete;
    EPollPoller(EPollPoller&&)                 = delete;
    auto operator=(const EPollPoller&) -> EPollPoller& = delete;
    auto operator=(EPollPoller&&) -> EPollPoller&      = delete;

    explicit EPollPoller(EventLoop* loop);
    ~EPollPoller() ;

    auto poll(int timeoutMs, ChannelList& activeChannels)
        ->  Timestamp;


    template <typename Rep, typename Period>
    auto poll(TimeDuration<Rep, Period> timeout, ChannelList& activeChannels)
        -> TimePoint
    {
        auto timeoutMs = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
        return poll(static_cast<int>(timeoutMs), activeChannels).toTimePoint();
    }
    /**
     * @attention called in owener loop thread
     * @brief update channel state and register events
     */
    void updateChannel(Channel *channel) ;

    /**
     * @attention 1. called in owener loop thread  2.channel must already be added in this poller and no events registered
     * @brief remove channel from poller
     */
    void removeChannel(Channel *channel) ;

    // 判断参数channel是否在当前的Poller当中
    auto hasChannel(Channel* channel) const
        -> bool;

    static auto operationToString(int op)
        ->  std::string_view;

private:

    void assertInOwnerThread_() const
    {
        owner_loop_->assertInOwnerThread();
    }

    inline static constexpr int c_k_init_event_list_size = 16;

    /**
     * @brief 填充活跃的连接到 activeChannels
     */
    void fillActiveChannels_(int eventNum, ChannelList &activeChannels) const;
    /**
     * @brief 更新channel通道 其实就是调用epoll_ctl
     */
    void update_(int operation, Channel *channel);

    using EventList = std::vector<struct epoll_event>; 


private:
    // map的key:sockfd value:sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;
    EventLoop *owner_loop_; // 定义Poller所属的事件循环EventLoop

    int epollfd_;      // epoll_create创建返回的fd保存在epollfd_中
    EventList events_; // 用于存放epoll_wait返回的所有发生的事件的文件描述符事件集
};