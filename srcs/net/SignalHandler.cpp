#include "net/SignalHandler.h"
#include <cstring>
#include <signal.h>
#include "net/Timestamp.h"
#include "sys/signalfd.h"
#include "logger/EasyLog.h"
#include <latch>

SignalHandler::SignalHandler(EventLoop* loop)
    : owner_loop_ {loop}
    , signal_fd_ {-1}
{
    sigemptyset(&mask_);
}

SignalHandler::~SignalHandler()
{

    // 确保 channel 在 owner loop 注销其状态
    if (signal_fd_ >= 0)
    {
        auto latch = std::latch {1};
        owner_loop_->runInLoop([this, &latch]() {
            channel_->diableAll();
            channel_->remove();
            latch.count_down();
        });
        latch.wait();
        ::close(signal_fd_);
    }
}

void SignalHandler::addSignal(int signo)
{
    sigaddset(&mask_, signo);
}

void SignalHandler::setSignalChannelReadCB(SignalCallback cb)
{
    signal_callback_ = std::move(cb);
}

void SignalHandler::startInLoop_()

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
    // channel_->setReadCallback([this](Timestamp) {
    //     this->signalChannelReadCB_();
    // });
    channel_->setHandler(this);
    channel_->enableReading();
    

    LOG_INFO_FMT(log, "SignalHandler started");
}

void SignalHandler::handleRead_(Timestamp)
{
    while (true)
    {
        auto si = signalfd_siginfo {};
        auto n  = ::read(signal_fd_, &si, sizeof(si));

        if (n == sizeof(si))
        {
            auto signo = si.ssi_signo;
            EASY_DEBUG("Received signal: {}:{}", strsignal(signo), signo);

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

void SignalHandler::restoreSignalsInLoop()
{
    owner_loop_->runInLoop([this]() {
        // 1. 停止 Channel 监听，防止继续从 signal_fd 读取
        if (channel_)
        {
            channel_->diableAll();
            channel_->remove();
        }

        // 2. 解除信号阻塞 (Unblock)
        // 这样信号产生时，系统会执行默认的处置动作（如 Terminate）
        if (::pthread_sigmask(SIG_UNBLOCK, &mask_, nullptr) < 0)
        {
            LOG_ERROR_FMT(log, "pthread_sigmask UNBLOCK failed");
        }

        // 3. (可选) 显式将信号重置为默认处理函数 SIG_DFL
        // 这是为了防止在 block 之前程序已经设置了自定义的 signal handler
        for (int i = 1; i < NSIG; ++i)
        {
            if (sigismember(&mask_, i))
            {
                struct sigaction sa;
                std::memset(&sa, 0, sizeof(sa));
                sa.sa_handler = SIG_DFL;
                ::sigaction(i, &sa, nullptr);
            }
        }

        // 4. 关闭文件描述符
        if (signal_fd_ >= 0)
        {
            ::close(signal_fd_);
            signal_fd_ = -1;
        }

        LOG_INFO_FMT(log, "SignalHandler stopped and signals restored to default");
    });
}