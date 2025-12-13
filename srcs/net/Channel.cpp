#include "net/Channel.h"
#include "net/EventLoop.h"

#include <sys/epoll.h>

// #include "EventLoop.h"
// #include "Logger.h"

// EventLoop: ChannelList Poller
Channel::Channel(EventLoop* loop, int fd)
    : loop_ {loop}
    , fd_ {fd}
    , registered_events_ {EventEnum::NoneEvent}
    , received_events_ {EventEnum::NoneEvent}
    , state_ {State::New}
    , tied_ {false}
{
}

// channel的tie方法什么时候调用过?  TcpConnection => channel
/**
 * TcpConnection中注册了Chnanel对应的回调函数，传入的回调函数均为TcpConnection
 * 对象的成员方法，因此可以说明一点就是：Channel的结束一定早于TcpConnection对象！
 * 保证epoll返回时,此处用tie去解决TcoConnection和Channel的生命周期时长问题，从而保证了Channel对象能够在TcpConnection销毁前销毁。
 **/
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_  = obj;
    tied_ = true;
}

/**
 * 当改变channel所表示的fd的events事件后，update负责再poller里面更改fd相应的事件epoll_ctl
 **/
void Channel::update_()
{
    // 通过channel所属的eventloop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    // 如果 Channel 与某个对象（通常是 TcpConnection）绑定了生命周期
    if (tied_)
    {
        // 防止handleEvent执行过程中 该对象被销毁
        auto guard = tie_.lock();
        if (guard)
        {
            // make sure the channel still exists with guard object
            HandleEventWithGuard_(receiveTime);
        }
        // 如果 guard 为空，说明对象已销毁，Channel 不再处理事件
    }
    // 如果没有绑定对象，直接处理事件
    else
    {
        HandleEventWithGuard_(receiveTime);
    }
}

void Channel::HandleEventWithGuard_(Timestamp receiveTime)
{
    // LOG_INFO("channel handleEvent revents:%d\n", revents_);
    // 关闭

    // POLLHUP 表示 对端完全关闭了连接, 如果同时有 POLLIN，说明还有数据没读完，本端先就不急着处理关闭。
    // 优先读数据（因为有 POLLIN），等读完再交给上层决定是否关闭。
    // 当TcpConnection对应Channel 通过shutdown 关闭写端 epoll触发EPOLLHUP
    if ((received_events_ & EPOLLHUP) != NoneEvent and !((received_events_ & EPOLLIN) != NoneEvent))
    {
        if (close_callback_)
        {
            close_callback_();
        }
    }

    // 这里只打个日志，提醒开发者逻辑有问题。
    // POLLNVAL 表示 fd 无效（poll 会对传入的fd逐个检查是否非法，比如被关闭后还传给 poll的fd等）。
    // 因为 POLLNVAL 的出现代表了程序逻辑有 bug：poll 监听了一个已经关闭或非法的 fd。
    // 所以不触发回调是合理的，避免干扰正常逻辑。

    // 为什么epoll中没有 EPOLLNVAL 事件?
    // 因为在 epoll_ctl 添加一个非法的 fd 时，epoll_ctl 会直接返回错误，不会成功添加到 epoll 实例中，
    // 也就不会再 poll 阶段触发 EPOLLNVAL 事件
    // if (revents_ & EPOLLNVAL)
    // {
    // //   LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
    // }

    // 错误事件
    // POLLERR 表示发生了错误（如peer send rst连接重置、local写失败等）
    if ((received_events_ & EPOLLERR) != NoneEvent)
    {
        if (error_callback_)
        {
            error_callback_();
        }
    }
    // 读事件
    // POLLIN：普通可读事件（socket buffer 里有数据）。
    // POLLPRI：紧急数据可读（如 TCP OOB 数据）。
    // POLLRDHUP：对端关闭了写端（半关闭，常用于检测 TCP 连接断开）。
    if ((received_events_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) != NoneEvent)
    {
        if (read_callback_)
        {
            read_callback_(receiveTime);
        }
    }
    // 写
    if ((received_events_ & EPOLLOUT) != NoneEvent)
    {
        if (write_callback_)
        {
            write_callback_();
        }
    }
}
