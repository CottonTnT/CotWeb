#include <algorithm>
#include <cassert>
#include <cerrno>
#include <ranges>
#include <unistd.h>

#include "net/Epoller.h"
// #include "Logger.h"
#include "net/Channel.h"


EPollPoller::EPollPoller(EventLoop* loop)
    : owner_loop_(loop)
    , epollfd_ {::epoll_create1(EPOLL_CLOEXEC)}
    , events_ {c_k_init_event_list_size} // vector<epoll_event>(16)
{
    if (epollfd_ < 0)
    {
        // LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

auto EPollPoller::poll(int timeoutMs, ChannelList& activeChannels)
    -> Timestamp
{
    // 由于频繁调用poll 实际上应该用LOG_DEBUG输出日志更为合理 当遇到并发场景 关闭DEBUG日志提升效率
    // LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());
    auto num_events = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);

    auto save_errno = errno;

    auto now = Timestamp::Now();

    if (num_events > 0)
    {
        // todo:log
        //  LOG_INFO("%d events happend\n", numEvents); // LOG_DEBUG最合理
        //  转就绪事件为 activeChannels
        fillActiveChannels_(num_events, activeChannels);
        if (num_events == events_.size()) // 扩容操作
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (num_events == 0)
    {
        // todo:log
        //  LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else
    {
        if (save_errno != EINTR)
        {
            errno = save_errno;
            // LOG_ERROR("EPollPoller::poll() error!");
            // todo:log
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
void EPollPoller::updateChannel(Channel* channel)
{
    assertInOwnerThread_();
    auto index = channel->getState();
    // LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), index);

    auto fd = channel->getFd();
    switch (index)
    {
        case Channel::State::New: {
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
            channel->setState(Channel::State::Listening);
            update_(EPOLL_CTL_ADD, channel);
        }
        case Channel::State::Listening: {
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
            if (channel->isNoneEvent()) // channel 不再关注任何事件, 即从epoll中删除该channel, 但channel还在 Epoller 的管理中
            {
                update_(EPOLL_CTL_DEL, channel);
                channel->setState(Channel::State::NoEventRegistered);
            }
            else
            {
                update_(EPOLL_CTL_MOD, channel);
            }
            break;
        }
        case Channel::State::NoEventRegistered: {
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
            channel->setState(Channel::State::Listening);
            update_(EPOLL_CTL_ADD, channel);
            break;
        }
        default:
            // LOG_FATAL("EPollPoller::updateChannel() bad channel state");
            break;
    }
}

void EPollPoller::removeChannel(Channel* channel)
{
    assertInOwnerThread_();
    auto fd = channel->getFd();
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    // thd fd of channel must not be interested in any events before channel removed from Epoller
    assert(channel->isNoneEvent());
    // LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);
    auto state = channel->getState();
    assert(state == Channel::State::Listening or  state == Channel::State::NoEventRegistered);

    auto removed_number = channels_.erase(fd);
    assert(removed_number == 1);

    if (state == Channel::State::Listening)
    {
        update_(EPOLL_CTL_DEL, channel);
    }
    channel->setState(Channel::State::New);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels_(int eventNum, ChannelList& activeChannels) const
{
    std::ranges::for_each(std::views::iota(0, eventNum), [this, &activeChannels](int idx) {
        // 注册时将 Channel* 存放在 epoll_event 的 data.ptr 中
        auto* channel = static_cast<Channel*>(events_[idx].data.ptr);
        // 设置 Channel 实际发生的事件
#ifndef NDEBUG
    int fd = channel->getFd();
    auto it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
#endif
        channel->setReceivedEvents(events_[idx].events);
        activeChannels.push_back(channel);
    });
}

void EPollPoller::update_(int operation, Channel* channel)
{
    assertInOwnerThread_();
    auto  event = epoll_event{};

    auto fd = channel->getFd();
    event.events   = channel->getRegisteredEvents();
    event.data.fd  = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            //
            // LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            // LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}

auto EPollPoller::hasChannel(Channel* channel) const
    -> bool
{
    return channels_.contains(channel->getFd());

}

auto EPollPoller::operationToString(int op)
    -> std::string_view
{
    switch (op)
    {
        case EPOLL_CTL_ADD:
            return "ADD";
        case EPOLL_CTL_DEL:
            return "DEL";
        case EPOLL_CTL_MOD:
            return "MOD";
        default:
            assert(false && "ERROR op");
            return "Unknown Operation";
    }
}