#include "common/util.h"
#include <dirent.h>
#include <pthread.h>
#include <string>
#include <syscall.h>
#include <unistd.h>


namespace UtilT {

auto GetElapseMs()
    -> uint64_t
{
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

auto GetThreadId()
    -> pid_t
{
    return syscall(SYS_gettid);
}

auto GetThreadName()
    -> std::string
{
    auto thr_name = std::array<char, 16>();
    thr_name.fill('\0');
    pthread_getname_np(pthread_self(), thr_name.data(), 16);
    return std::string(thr_name.data());
}


CallGuard::CallGuard(Callback cb)
    : call_(cb)
{
}

CallGuard::~CallGuard()
{
    call_();
}

}