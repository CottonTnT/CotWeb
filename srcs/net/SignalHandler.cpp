#include "net/SignalHandler.h"
#include <cstring>
#include <signal.h>
#include "net/Timestamp.h"
#include "sys/signalfd.h"
#include "logger/EasyLog.h"


SignalHandler::SignalHandler(EventLoop* loop)
    : owner_loop_ {loop}
    , signal_fd_ {-1}
{
    sigemptyset(&mask_);
}

SignalHandler::~SignalHandler()
{
    if (signal_fd_ >= 0)
    {
        ::close(signal_fd_);
    }
}

void SignalHandler::addSignal(int signo)
{
    sigaddset(&mask_, signo);
}

void SignalHandler::setCallback(SignalCallback cb)
{
    signal_callback_ = std::move(cb);
}

void SignalHandler::start()
{
    // 1️⃣ 阻塞这些信号（防止默认 handler 生效）
    if (::pthread_sigmask(SIG_BLOCK, &mask_, nullptr) < 0)
    {
        LOG_SYSFATAL_FMT(log, "pthread_sigmask failed");
    }

    // 2️⃣ 创建 signalfd
    signal_fd_ = ::signalfd(-1, &mask_, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signal_fd_ < 0)
    {
        LOG_SYSFATAL_FMT(log, "signalfd failed");
    }

    // 3️⃣ 注册进 EventLoop
    channel_ = std::make_unique<Channel>(owner_loop_, signal_fd_);
    channel_->setReadCallback([this](Timestamp) {
        this->signalChannelReadCB_();
    });
    channel_->enableReading();

    LOG_INFO_FMT(log, "SignalHandler started");
}

void SignalHandler::signalChannelReadCB_()
{
    while (true)
    {
        auto si =  signalfd_siginfo{};
        auto n = ::read(signal_fd_, &si, sizeof(si));

        if (n == sizeof(si))
        {
            auto signo = si.ssi_signo;
            EASY_DEBUG("Received signal: {}:{}", strsignal(signo),signo);

            if (signal_callback_)
            {
                signal_callback_(signo);
            }
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            EASY_ERROR("signalfd read error");
            break;
        }
    }
    
}
