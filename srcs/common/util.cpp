#include "common/util.h"
#include "common/util.hpp"
#include <dirent.h>
#include <memory>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <syscall.h>
#include <unistd.h>
#include <execinfo.h>


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


/**
 * @brief too ugly to touch, just copy it here
 */
static auto Demangle(const char* str)
    -> std::string
{
    size_t size = 0;
    int status = 0;
    std::string rt;
    rt.resize(256);
    if(1 == sscanf(str, "%*[^(]%*[^_]%255[^)+]", &rt[0])) {
        char* v = abi::__cxa_demangle(&rt[0], nullptr, &size, &status);
        if(v) {
            std::string result(v);
            free(v);
            return result;
        }
    }    
    if(1 == sscanf(str, "%255s", &rt[0])) {
        return rt;
    }
    return str;
}


void Backtrace(std::vector<std::string>& bt,
               std::uint32_t size,
               std::uint32_t skip)
{
    auto stk_arr = std::unique_ptr<void*>
    {
        (void**)malloc((sizeof(void*)) * size)
    };

    auto sz = backtrace(stk_arr.get(), size);

    auto strings_or = SyscallWrapper<nullptr>(backtrace_symbols,
                                              stk_arr.get(),
                                              sz);

    if (not strings_or.has_value()) [[unlikely]]
    {
        // throw std::runtime_error{strings_or.error()}; can`t not throw here
        //todo: do something else ?
        return;
    }

    auto strings = std::unique_ptr<char*>{strings_or.value()};

    for (auto i = skip; i < sz; i++)
    {
        bt.push_back(Demangle(strings.get()[i]));
    }
}


auto BacktraceToString(std::uint32_t size,
                       std::uint32_t skip,
                       const std::string& prefix)
    -> std::string
{
    auto bt = std::vector<std::string>{};
    Backtrace(bt, size, skip);
    auto sstream = std::stringstream{};
    for (auto i = skip; i < 5; i++)
    {
        sstream << prefix << bt[i] << "\n";
    }
    return sstream.str();
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