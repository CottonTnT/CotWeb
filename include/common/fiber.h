#pragma once

#include "alias.h"
#include "curthread.h"
#include "fiberstate.h"

#include <functional>
#include <memory>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <utility>
// #include "fiber/scheduler.h"
namespace FiberT {

class Fiber : public std::enable_shared_from_this<Fiber> {
    friend class Scheduler;

    friend auto  CurThr::Resume(Sptr<Fiber> fiber)
        -> void;
    friend auto  CurThr::YieldToReady()
        -> void;

    friend auto  CurThr::YieldToHold()
        -> void;

    friend auto  CurThr::YieldToExcept()
        -> void;

    friend auto  CurThr::YieldToTerm()
        -> void;
private:
    /**
     * @attention  only for construct of the root Fiber of every thread using the stack of thread, i.e, by default the main fiber which also the schedule fiber of every thread,
    and we never care about the state of root fiber 
     */
    Fiber();

    /**
     * @brief  construct the fiber with it`s private context
     * @param[in] cb 协程入口函数
     * @param[in] stacksize 栈大小
     * @param[in] run_in_scheduler 本协程是否参与调度器调度，默认为true
     */
    Fiber(std::function<void()> cb,
          uint32_t stk_size = 0);

    template <typename... Args>
    static auto CreateFiberImpl_(Args&&... args)
        -> std::shared_ptr<Fiber>
    {
        return std::shared_ptr<Fiber>(new Fiber {std::forward<Args>(args)...});
    }

    static auto CreateMainFiber_()
        -> Sptr<Fiber>;

    /**
     * @brief main fiber, by default the root fiber, can change the current thread main fiber by CurThr::SetMainFiber_() 
     */
    auto IsMainFiber_() const
        -> bool;

    /**
     * @brief root fiber, the first fiber of every thread which use the stack of thread
     */
    auto IsRootFiber_() const
        -> bool;

    /**
     * @brief 切换主协程到后台，将当前协程切换到运行状态, 由于我们的协程是非对称的，只允许子协程与主协程之间的切换
     * @attention only sub fiber will call this
     * @pre getState() != EXEC
     * @post getState() = EXEC
     */
    void Resume_();

    /**
     * @brief 将当前协程切换到后台,并设置为READY状态
     * @post getState() = READY
     */
    void YieldTo_(FiberState next_state);
public:
    Fiber(const Fiber&)                    = delete;
    Fiber(Fiber&&)                         = delete;
    auto operator=(const Fiber&) -> Fiber& = delete;
    auto operator=(Fiber&&) -> Fiber&      = delete;

    ~Fiber();

    /**
     * @brief 重置协程执行函数,并设置状态
     * @pre getState() 为, TERM, EXCEPT
     * @post getState() READY, UNUSED 
     */
    auto Reset(std::function<void()> cb) // 复用已创建的内存，开启新的fiber
        -> void;


    /**
     * @brief 返回协程id
     */
    auto GetId() const
        -> uint64_t { return id_; }

    /**
     * @brief 返回协程状态
     */
    auto GetState() const
        -> FiberState { return state_; }

    auto SetState(FiberState new_state)
        -> void { state_ = new_state; }


    /**
     * @details will create the main fiber if it doesn`t exits
     */
    static auto CreateFiber(std::function<void()> cb,
                            uint32_t stk_size = 0)
        -> std::shared_ptr<Fiber>;

    /**
     * @brief get the running fiber number
     */
    static auto GetTotalFibers()
        -> uint64_t;

    /**
     * @pre the current running fiber is set already
     * @brief 协程执行函数
     * @post 执行完成返回到线程调度协程
     */
    static void CallBackWrapper();

    static inline constexpr auto c_DefaultStkSize = uint32_t {256 * 1024};

    static inline constexpr auto c_NonUse = uint64_t{0};
private:
    uint64_t id_;
    uint32_t stk_size_ = 0;
    FiberState state_  = FiberState::UNUSED;
    ucontext_t ctx_; // 协程上下文
    std::unique_ptr<void, void (*)(void*)> stk_; // 协程栈地址
    std::function<void()> callback_; // 协程
};

} // namespace FiberT

