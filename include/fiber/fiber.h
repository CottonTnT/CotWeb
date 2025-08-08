#pragma once

#include <functional>
#include <memory>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <utility>

#include "common/alias.h"
#include "fiber/fiberstate.h"
// #include "fiber/scheduler.h"

namespace FiberT {

class Fiber : public std::enable_shared_from_this<Fiber> {
    friend class Scheduler;

private:
    /**
     * @attention  only for construct of the first Fiber of every thread, i.e, main fiber,and we never care about the state of main fiber ,cause we never user it for now
     */
    Fiber();

    /**
     * @brief only for construct the sub fiber
     * @param[in] cb 协程入口函数
     * @param[in] stacksize 栈大小
     * @param[in] run_in_scheduler 本协程是否参与调度器调度，默认为true
     */
    Fiber(std::function<void()> cb,
          std::uint32_t stk_size = 0);

    template <typename... Args>
    static auto CreateFiberImpl_(Args&&... args)
        -> std::shared_ptr<Fiber>
    {
        return std::shared_ptr<Fiber>(new Fiber {std::forward<Args>(args)...});
    }

    auto IsMainFiber_() const
        -> bool;

public:
    Fiber(const Fiber&)                    = delete;
    Fiber(Fiber&&)                         = delete;
    auto operator=(const Fiber&) -> Fiber& = delete;
    auto operator=(Fiber&&) -> Fiber&      = delete;

    /**
     * @brief 构造函数
     * @param[in] cb 协程执行的函数
     * @param[in] stk_size 协程栈大小
     * @param[in] use_caller 是否在MainFiber上调度
     */
    ~Fiber();

    /**
     * @brief 重置协程执行函数,并设置状态
     * @pre getState() 为 INIT, TERM, EXCEPT
     * @post getState() = INIT
     */
    auto Reset(std::function<void()> cb) // 复用已创建的内存，开启新的fiber
        -> void;

    /**
     * @brief 切换主协程到后台，将当前协程切换到运行状态, 由于我们的协程是非对称的，只允许子协程与主协程之间的切换
     * @pre getState() != EXEC
     * @post getState() = EXEC
     */
    void Resume();

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
     * @brief 将当前协程切换到后台,并设置为READY状态
     * @post getState() = READY
     */
    void YieldTo(FiberState next_state);

    /**
     * @details will create the main fiber if it doesn`t exits
     */
    static auto CreateFiber(std::function<void()> cb,
                            std::uint32_t stk_size = 0)
        -> std::shared_ptr<Fiber>;

    /**
     * @brief 返回当前协程的总数量
     */
    static auto GetTotalFibers()
        -> uint64_t;

    /**
     * @pre the current running fiber is set already
     * @brief 协程执行函数
     * @post 执行完成返回到线程调度协程
     */
    static void CallBackWrapper();

    static inline constexpr auto c_DefaultStkSize = std::uint32_t {8096};

private:
    uint64_t id_;
    uint32_t stk_size_ = 0;
    FiberState state_  = FiberState::INIT;
    ucontext_t ctx_; // 协程上下文
    std::unique_ptr<void, void (*)(void*)> stk_; // 协程栈地址
    std::function<void()> callback_; // 协程
    // bool run_in_scheduler_;
};

} // namespace FiberT

namespace CurThr {

auto GetRunningFiberId()
    -> std::optional<uint64_t>;

auto YieldToReady()
    -> void;

auto YieldToHold()
    -> void;

auto YieldToExcept()
    -> void;

auto YieldToTerm()
    -> void;

auto Resume(Sptr<FiberT::Fiber> fiber)
    -> void;

} // namespace CurThr
