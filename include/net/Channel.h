#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "Timestamp.h"

#include <sys/epoll.h>
class EventLoop;

namespace {

// 定义各种成员函数的探测概念
template <typename T>
concept HasHandleRead = requires(T t, Timestamp ts) {
    { t.handleRead_(ts) } -> std::same_as<void>;
};

template <typename T>
concept HasHandleWrite = requires(T t) {
    { t.handleWrite_() } -> std::same_as<void>;
};

template <typename T>
concept HasHandleClose = requires(T t) {
    { t.handleClose_() } -> std::same_as<void>;
};

template <typename T>
concept HasHandleError = requires(T t) {
    { t.handleError_() } -> std::same_as<void>;
};

// 综合概念：只要实现其中任何一个回调，就可以被视为有效的处理器
template <typename T>
concept ChannelHandler = HasHandleRead<T> or HasHandleWrite<T> or HasHandleClose<T> or HasHandleError<T>;
} // namespace

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

private:
    // 内部抽象基类：定义事件接口
    struct EventHandlerFacade
    {
        virtual ~EventHandlerFacade()  = default;
        virtual void onRead(Timestamp) = 0;
        virtual void onWrite()         = 0;
        virtual void onClose()         = 0;
        virtual void onError()         = 0;
    };

    // 内部模板实现类：类型擦除的关键
    template <typename T>
    struct EventHandlerProxy : public EventHandlerFacade
    {
        EventHandlerProxy(T* obj)
            : instance_(obj)
        {
        }

        void onRead(Timestamp t) override
        {
            if constexpr (HasHandleRead<T>)
            {
                instance_->handleRead_(t);
            }
        }
        void onWrite() override
        {
            if constexpr (HasHandleWrite<T>)
            {
                instance_->handleWrite_();
            }
        }
        void onClose() override
        {
            if constexpr (HasHandleClose<T>)
            {
                instance_->handleClose_();
            }
        }
        void onError() override
        {
            if constexpr (HasHandleError<T>)
            {
                instance_->handleError_();
            }
        }
        T* instance_; // 指向 TcpConnection 或 Acceptor 等
    };

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

    EventLoop* const loop_;      // 事件循环
    const int fd_;               // fd，Poller监听的对象, socket, eventfd, timerfd
    uint32_t registered_events_; // 注册fd感兴趣的事件
    uint32_t received_events_;   // Poller返回的具体发生的事件
    State state_;                // channel在Poller中的状态 标识channel是否已经添加到Poller中以及是否有事件注册

    std::unique_ptr<EventHandlerFacade> handler_; // 类型擦除后的句柄
    std::weak_ptr<void> tie_;
    bool tied_;
    bool event_handling_;
    bool added_to_loop_;
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
    // void handleEvent(Timestamp receiveTime);
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
    void diableAll()
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

    void handleEvent(Timestamp receiveTime);

    // 使用 concept 约束模板参数，确保传进来的类是合法的
    template<ChannelHandler T>
    void setHandler(T* obj) {
        handler_ = std::make_unique<EventHandlerProxy<T>>(obj);
    }

private:
    void handleEventWithGuard_(Timestamp receiveTime);
};