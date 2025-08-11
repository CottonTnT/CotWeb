#pragma  once


#include "countdownlatch.h"

#include <functional>
#include <string>

namespace Thr{

class Thread {
public:
    static inline constexpr pthread_t c_NullThread = 0;
    Thread(std::function<void()> cb, std::string name);

    Thread(const Thread&) = delete;
    auto operator=(const Thread&)
        -> Thread& = delete;

    Thread(Thread&&) noexcept;
    auto operator=(Thread&&) noexcept
        -> Thread& ;

    ~Thread();

    auto Join()
        -> void;

    auto GetId() const
        -> pid_t { return id_;}
    auto GetName() const
        -> std::string_view{ return name_;}

    static  auto CallBackWrapper(void* arg)
        -> void*;

    static auto GetCurThreadHandle()
        -> Thread*;

private:
    pid_t id_ = -1;
    pthread_t thread_ = c_NullThread;
    std::function<void()> cb_;
    std::string name_;
    Cot::CountDownLatch latch_{1};
};
} //namespace Thr