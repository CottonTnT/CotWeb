#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "Timestamp.h"

#include <sys/epoll.h>
class EventLoop;

/**
 * @attention
 * 1.one channel only belongs to one IO thread
 * 2.one channel only is responsible for one fd
 * 3.it doesn`t owns fd, i.e it won`t close it when it destructs
 *
 * Channel 是所有 fd owner 连接 EPollPoller通道,
 * 其他 fd owner 通过 channel 与 EPollPoller打交道, e.g. Tcpconnection, TimerQueue,封装了 sockfd 和其感兴趣的 event 如EPOLLIN、EPOLLOUT事件,负责 IO事件的分发, 时间发生后调用相应的回调操作
 **/
class Channel {

public:
    enum EventEnum {
        NoneEvent  = 0,
        ReadEvent  = EPOLLIN | EPOLLPRI,
        WriteEvent = EPOLLOUT,
    };
    using EventCallback = std::function<void()>;

    using ReadEventCallback = std::function<void(Timestamp)>;
    enum State {
        // channel里的 fd 还没添加至Poller, channel本身也还没添加至EPoller
        New = -1,
        // channel里的 fd 已添加至Poller注册了感兴趣的事件, 某个channel已经添加至EPoller, 且
        Listening = 1,
        // channel里的 fd 已从Poller里删除,  但还 channel 还没从 EPoller
        NoEventRegistered = 2,
    };
private:
    void update_();
    void HandleEventWithGuard_(Timestamp receiveTime);

    EventLoop* loop_;            // 事件循环
    const int fd_;               // fd，Poller监听的对象, socket, eventfd, timerfd
    uint32_t registered_events_; // 注册fd感兴趣的事件
    uint32_t received_events_;   // Poller返回的具体发生的事件
    State state_; //channel在Poller中的状态 标识channel是否已经添加到Poller中以及是否有事件注册

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为channel通道里可获知fd最终发生的具体的事件events，所以它负责调用具体事件的回调操作
    ReadEventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback close_callback_;
    EventCallback error_callback_;

public:

    Channel(EventLoop* loop, int fd);

    Channel(const Channel&) = delete;
    auto operator=(const Channel&)
        -> Channel& = delete;

    Channel(Channel&&) = delete;
    auto operator=(Channel&&)
        -> Channel& = delete;

    ~Channel() = default;

    /**
     * @brief  fd得到 Poller 通知以后 处理事件 handleEvent 在EventLoop::loop()中调用
     */
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { read_callback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { write_callback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { close_callback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { error_callback_ = std::move(cb); }

    /**
     * @brief Tie this channel to the owner object managed by shared_ptr,prevent the owner object being destroyed in handleEvent.
     */
    void tie(const std::shared_ptr<void>&);

    [[nodiscard]] auto getFd() const
        -> int { return fd_; }
    [[nodiscard]] auto getRegisteredEvents() const -> uint32_t { return registered_events_; }

    void setReceivedEvents(uint32_t revt) { received_events_ = revt; }

    // 设置fd相应的事件状态 相当于epoll_ctl add delete
    void enableReading()
    {
        registered_events_ |= EventEnum::ReadEvent;
        update_();
    }
    void diableReading()
    {
        registered_events_ &= ~EventEnum::ReadEvent;
        update_();
    }
    void enableWriting()
    {
        registered_events_ |= EventEnum::WriteEvent;
        update_();
    }
    void diableWriting()
    {
        registered_events_ &= ~EventEnum::WriteEvent;
        update_();
    }
    void unregisterAllEvent()
    {
        registered_events_ = EventEnum::NoneEvent;
        update_();
    }

    // 返回fd当前的事件状态
    [[nodiscard]] auto isNoneEvent() const
        -> bool { return registered_events_ == EventEnum::NoneEvent; }

    [[nodiscard]] auto isWriting() const
        -> bool { return (registered_events_ & EventEnum::WriteEvent) != EventEnum::NoneEvent; }

    [[nodiscard]] auto isReading() const
        -> bool { return (registered_events_ & EventEnum::ReadEvent) != EventEnum::NoneEvent; }

    [[nodiscard]] auto getState() const
        -> State { return state_; }

    void setState(State state) { state_ = state; }

    // one loop per thread
    auto getOwnerLoop()
        -> EventLoop* { return loop_; }
    /**
     * @brief  在channel所属的EventLoop中把当前的channel删除掉
     */
    void remove();

};