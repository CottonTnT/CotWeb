#pragma once

#include <functional>
#include <memory>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <utility>

#include "common/alias.h"
#include "fiber/fiberstate.h"
// #include "fiber/scheduler.h"


namespace FiberT{

class Fiber : public std::enable_shared_from_this<Fiber>{
friend class Scheduler;
private:
    /**
     * @brief No Args constructor
     * @attention  only for construct of the first Fiber of every thread, i.e, main fiber
     */
    Fiber();

    /**
     * @brief for construct the sub fiber of thread, and we never care about the state of main fiber ,cause we never user it for now
     */
    Fiber(std::function<void()> cb,
          std::uint32_t stk_size = 0);
    
    template <typename... Args>
    static auto CreateFiber_(Args&&... args)
        -> std::shared_ptr<Fiber>
    {
        return std::shared_ptr<Fiber>(new Fiber{std::forward<Args>(args)...});
    }

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


    /**
     * @brief 是否已中止(正常结束或异常）
     */
    auto IsTerminated()
        -> bool
    {
        return state_ == FiberState::TERM;
    }

    /**
     * @brief 是否就绪
     */
    auto IsReady()
        -> bool
    {
        return state_ == FiberState::READY;
    }

    /**
     * @brief 将当前协程切换到后台,并设置为READY状态
     * @post getState() = READY
     */
    void Yield();


    static auto CreateMainFiber()
        -> void;

    static auto CreateSubFiber(std::function<void()> cb,
                               std::uint32_t stk_size = 0)
        -> std::shared_ptr<Fiber>;

    /**
     * @brief set the running fiber of current thread
     */
    static auto SetCurThrRunningFiber(Fiber* f)
        -> void;

    /**
     * @brief 返回当前线程正在执行的协程
     * @details 如果当前线程还未创建协程，则创建线程的第一个协程，
     * 且该协程为当前线程的主协程，其他协程都通过这个协程来调度，也就是说，其他协程
     * 结束时,都要切回到主协程，由主协程重新选择新的协程进行resume
     * @attention 线程如果要创建协程，那么应该首先执行一下Fiber::GetThis()操作，以初始化主函数协程
     */
    static auto GetCurThrRunningFiber()
        -> Sptr<Fiber>;
    
    static auto GetCurThrRunningFiberId()
        -> uint64_t;




    /**
     * @brief 返回当前协程的总数量
     */
    static auto GetTotalFibers()
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
    static void FiberMainFunc();

    static inline constexpr auto c_DefaultStkSize = std::uint32_t{8096};
private:
    uint64_t id_;
    uint32_t stk_size_ = 0;
    FiberState state_;
    ucontext_t ctx_; //协程上下文

    std::unique_ptr<void, void(*)(void*)> stk_; //协程栈地址

    std::function<void()> cb_; //协程
};
} //namespace FiberT