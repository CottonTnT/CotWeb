#include "common/curthread.h"
#include "common/thread.h"
#include "common/util.hpp"
#include "logger/logger.h"
#include "logger/loggermanager.h"
#include <cerrno>
#include <pthread.h>
#include <stdexcept>
#include <utility>

namespace Thr {

static thread_local Thread* t_ThreadHandle   = nullptr;
static thread_local std::string t_ThreadName = "UNKNOWN";

static Sptr<LogT::Logger> g_Logger = GET_LOGGER_BY_NAME("system");

auto Thread::GetCurThreadHandle()
    -> Thread*
{
    return t_ThreadHandle;
}

Thread::Thread(std::function<void()> cb, std::string name)
    : cb_ {std::move(cb)}
    , name_ {std::move(name)}
{
    if (name_.empty())
    {
        name_ = "UNKNOWN";
    }

    auto ret = UtilT::SyscallWrapper<EAGAIN, EINVAL, EPERM>(pthread_create, &thread_, nullptr, &Thread::CallBackWrapper, this);
    if (not ret.has_value())
    {
        LOG_ERROR(g_Logger) << "pthread_create thread fail:" << ret.error() << name_;
        throw std::logic_error("pthread_create error");
    }
    latch_.Wait();
}

Thread::~Thread()
{
    if (thread_ != c_NullThread)
    {
        pthread_detach(thread_);
    }
}

auto Thread::Join()
    -> void
{
    if (thread_ != c_NullThread)
    {
        auto ret = UtilT::SyscallWrapper<EDEADLOCK, EINVAL, ESRCH>(pthread_join, thread_, nullptr);
        if (not ret)
        {
            LOG_ERROR(g_Logger) << "pthread_join thread fail:" << ret.error() << name_;
            throw std::logic_error("pthread_join error");
        }
        thread_ = c_NullThread;
    }
}

auto Thread::CallBackWrapper(void* arg)
    -> void*
{
    auto* thr      = static_cast<Thread*>(arg);
    t_ThreadHandle = thr;
    t_ThreadName   = thr->name_;

    thr->id_ = CurThr::GetId();

    //todo: figure pthread_setname_up out
    pthread_setname_np(pthread_self(), thr->name_.substr(0, 15).c_str());

    auto callback = std::exchange(thr->cb_, std::function<void()> {});
    thr->latch_.CountDown();

    callback();
    return nullptr;
}

} // namespace Thr