#pragma once

#include <functional>
#include <memory>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <utility>

#include "common/alias.h"
#include "fiber/fiberstate.h"


namespace FiberT{

class Fiber : public std::enable_shared_from_this<Fiber>{

private:
    /**
     * @brief the main fiber of thread, for construct of the first Fiber of every thread, i.e, main fiber
     */
    Fiber();

    /**
     * @brief for construct the sub fiber of thread
     */
    Fiber(std::function<void()> cb,
          std::uint32_t stk_size = 0,
          bool use_caller = false);
    
    template <typename... Args>
    static auto CreateFiber_(Args&&... args)
        -> std::shared_ptr<Fiber>
    {
        return std::shared_ptr<Fiber>(new Fiber{std::forward<Args>(args)...});
    }

    /**
     * @brief 将当前协程切换到后台，唤醒主协程, 不改变当前协程的状态
     */
    void BackToMain_(FiberState next_state);

    auto IsMainFiber_() const
        -> bool;
public:

    Fiber(const Fiber&)            = delete;
    Fiber(Fiber&&)                 = delete;
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
    auto Reset(std::function<void()> cb) //复用已创建的内存，开启新的fiber
        -> void;
    /**
     * @brief 切换主协程到后台，将当前协程切换到运行状态, 由于我们的协程是非对称的，只允许子协程与主协程之间的切换
     * @pre getState() != EXEC
     * @post getState() = EXEC
     */
    void Resume();


    /**
     * @brief 将当前线程切换到执行状态
     * @pre 执行的为当前线程的主协程
     */
    void Call();

    /**
     * @brief 将当前线程切换到后台
     * @pre 执行的为该协程
     * @post 返回到线程的主协程
     */
    void Back();

    /**
     * @brief 返回协程id
     */
    auto GetId() const
        -> uint64_t{ return id_;}

    /**
     * @brief 返回协程状态
     */
    auto GetState() const
        -> FiberState { return state_;}

    auto SetState(FiberState new_state)
        -> void { state_ = new_state; }

    static auto CreateMainFiber()
        -> void;

    static auto CreateSubFiber(std::function<void()> cb,
                               std::uint32_t stk_size = 0,
                               bool use_caller = false)
        -> std::shared_ptr<Fiber>;

    /**
     * @brief set the running fiber of current thread
     */
    static auto SetCurThrRunningFiber(Fiber* f)
        -> void;

    /**
     * @brief if there is no running fiber, then create the main fiber
     */
    static auto GetCurThrRunningFiber()
        -> Sptr<Fiber>;
    
    static auto GetCurThrRunningFiberId()
        -> uint64_t;


    /**
     * @brief 将当前协程切换到后台,并设置为READY状态
     * @post getState() = READY
     */
    static void YieldToReady();

    /**
     * @brief 将当前协程切换到后台,并设置为Hold状态
     * @post getState() = HOLD
     */

    static void YieldToHold();

    /**
     * @brief 返回当前协程的总数量
     */
    static auto TotalFibers()
        -> uint64_t;


    /**
     * @brief 协程执行函数
     * @post 执行完成返回到线程主协程
     */
    static void SubFiberMain();

    /**
     * @brief 协程执行函数
     * @post 执行完成返回到线程调度协程
     */
    static void CallerMainFunc();

    static inline constexpr auto c_DefaultStkSize = std::uint32_t{8096};
private:
    uint64_t id_ = 0;
    uint32_t stk_size_ = 0;
    FiberState state_ = FiberState::INIT;
    ucontext_t ctx_;
    std::unique_ptr<void, void(*)(void*)> stk_;
    std::function<void()> cb_;
};
} //namespace FiberT