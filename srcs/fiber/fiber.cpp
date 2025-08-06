#include "common/util.hpp"
#include "fiber/fiberexception.h"
#include "fiber/fiberstate.h"
#include "logger/logger.h"
#include "fiber/fiber.h"
#include "logger/loggermanager.h"
#include "fiber/fiberutil.h"

#include <atomic>
#include <boost/math/policies/policy.hpp>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <memory>
#include <ucontext.h>
#include <iostream>

namespace FiberT{


static Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER();

static thread_local auto t_RunningFiber = (Fiber*)nullptr;  //why not make it sptr? maybe for performance
static thread_local auto t_MainFiber = Sptr<Fiber>{nullptr};
static auto s_RunningFiberCount = std::atomic<uint64_t>{0};
static auto s_FiberId = std::atomic<uint64_t>{0};

// note: 踩坑,注意静态变量的析构顺序，保证g_logger在析构函数打印日志时，最后一个析构
// Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER(); 需定义为

static auto SetCurThrMainThread(Sptr<Fiber> fiber)
    -> void
{
    t_MainFiber = fiber;
}

class StackAllocator {
public:
    static auto Alloc(std::size_t size)
        -> void*
    {
        return malloc(size);
    }
    static auto Dealloc(void* vp)
        -> void
    {
        return free(vp);
    }

};

auto Fiber::GetCurThrRunningFiberId()
    -> uint64_t
{
    //因为有的线程不一定有协程
    return t_RunningFiber != nullptr ? t_RunningFiber->id_ : 0;
}

Fiber::Fiber()
    : id_ {s_FiberId++}
    , state_ {FiberState::RUNNING}
    , stk_
{
    nullptr, [](void*) -> void {
    }}
{
    assert(t_MainFiber == nullptr);

    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_);not success) [[unlikely]]
    {
        throw FiberInitError{"Init the main fiber failed -- " + success.error()};
    }

    SetCurThrRunningFiber(this);
    // SetCurThrMainThread(this->shared_from_this()); can`t call this->shared_from_this() here
    ++ s_RunningFiberCount;

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) <<  "Fiber::Fiber() main id = " << id_;
#endif
}

Fiber::Fiber(std::function<void()> cb,
             std::uint32_t stk_size) 
    : id_ { s_FiberId ++}
    , stk_size_ { stk_size == 0 ? c_DefaultStkSize : stk_size }
    , state_{FiberState::READY}
    , stk_ {StackAllocator::Alloc(stk_size_), StackAllocator::Dealloc}
    , cb_{ std::move(cb) }

{
    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_);
        not success) [[unlikely]]
    {
        throw FiberInitError{"Init the main fiber failed -- " + success.error()};
    }
    ++s_RunningFiberCount;

    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stk_.get();
    ctx_.uc_stack.ss_size = stk_size_;

    makecontext(&ctx_, FiberMainFunc,0);
    //todo:log here
#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) <<  "Fiber::Fiber() sub id = " << id_;
#endif
}

Fiber::~Fiber()
{
    std::cerr << "Fiber::~Fiber id=" << id_ << "FiberState = " << FiberStateToString(state_);
    #ifndef NO_DEBUG
        LOG_INFO(g_logger) << "Fiber::~Fiber id = " << id_ <<  "FiberState = " << FiberStateToString(state_);
    #endif

    -- s_RunningFiberCount;

    if (not IsMainFiber_())
    {
        
        #ifndef NO_DEBUG
            LOG_INFO(g_logger) << "FiberState::" << FiberStateToString(state_);
        #endif

        assert(state_ == FiberState::TERM);
    }
    else
    {
        assert(not cb_);
        assert(state_ == FiberState::RUNNING); //此时主线程一定是执行状态
        auto *cur = t_RunningFiber;

        assert(cur == this); //主协程释放时，其他子协程必须已经释放
        SetCurThrRunningFiber(nullptr);

        #ifndef NO_DEBUG
            LOG_INFO(g_logger) << "FiberState::" << FiberStateToString(state_);
        #endif
    }

    //todo: log here
}

auto Fiber::Reset(std::function<void()> cb)
    -> void
{
    // todo: assset here
    assert(IsMainFiber_()); //main fiber won`t call this function
    assert(state_ == FiberState::TERM);

    cb_ = std::move(cb);
    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_);not success) [[unlikely]]
    {
        throw FiberInitError{"reset the main fiber failed -- " + success.error()};
    }

    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stk_.get();
    ctx_.uc_stack.ss_size = stk_size_;
    
    SetState(FiberState::READY);
}


auto Fiber::CreateMainFiber()
    -> void
{
    auto main_fiber = CreateFiber_();
    SetCurThrMainThread(main_fiber->shared_from_this()); 
}

auto Fiber::CreateSubFiber(std::function<void()> cb,
                           std::uint32_t stk_size)
    -> std::shared_ptr<Fiber>
{
    return CreateFiber_(cb, stk_size);
}

auto Fiber::Resume()
    -> void
{
    assert(state_ != FiberState::RUNNING and state_ != FiberState::TERM);

    // talk about: what if a TERM fiber call resume ? how about just return ?
    // for scheduler in OS, never rerun the dead proc, so the developer should
    // responseble for the correctness cause you are the scheduler now!

    SetCurThrRunningFiber(this);
    SetState(FiberState::RUNNING);

    if (auto success = UtilT::SyscallWrapper<-1>(swapcontext, &t_MainFiber->ctx_, &ctx_); not success) [[unlikely]]
    {
        throw FiberSwapError{"swap out main fiber failed -- " + success.error()};
        //todo assert here
    }
}



void Fiber::SetCurThrRunningFiber(Fiber* f)
{
    t_RunningFiber = f;
}

auto Fiber::GetCurThrRunningFiber()
    -> Sptr<Fiber>
{
    if(t_RunningFiber == nullptr)
        CreateMainFiber();

    return t_RunningFiber->shared_from_this();
} 


auto Fiber::Yield()
    -> void
{
    /// 协程运行完之后会自动yield一次，用于回到主协程，此时状态已为结束状态
    assert(state_ == FiberState::RUNNING or state_ == FiberState::TERM);
    
    SetCurThrRunningFiber(t_MainFiber.get());

    switch (state_) {
        case FiberState::RUNNING:
            SetState(FiberState::READY);
            break;
        case FiberState::TERM:
            SetState(FiberState::TERM); // todo: how about set READY?
            break;
        default:
            LOG_ERROR(g_logger) << "Fiber::Yield: should never been here\n";
            assert(false);
    }

    if (auto success = UtilT::SyscallWrapper<-1>(swapcontext, &ctx_, &t_MainFiber->ctx_);
        not success) [[unlikely]]
    {
        throw FiberSwapError{"swap to the main fiber failed -- " + success.error()};
    }
    //talk about: how about set the ctx_->uc_link = t_MainFiber->ctx_ here?

}

auto Fiber::GetTotalFibers()
    -> uint64_t
{
    return s_RunningFiberCount;
}

/*
 * 这里应该删减处理协程函数出现异常的情况，此处只是为了好debug,故留下了
 * 个人认为协程的异常不应该由框架处理，应该由开发者自行处理,
 * 想想你平时的线程出现异常时难道不是你自己处理的吗? 难办? 就都别办了！！！
 */
auto Fiber::FiberMainFunc()
    -> void
{
    auto cur = GetCurThrRunningFiber();
    assert(cur != nullptr);
    try
    {
        cur->cb_();
        cur->cb_ = nullptr;
        cur->SetState(FiberState::TERM);
    }
    catch (...) 
    {
        cur->SetState(FiberState::TERM);
        LOG_ERROR(g_logger) << "Fiber Except: " 
            << " fiber_id=" << cur->GetId()
            << std::endl;
        std::terminate();
    }

    // -- s_RunningFiberCount; only ~Fiber() will do this, cause you may reuse this fiber before ~Fiber()
    auto* raw_ptr = cur.get();
    cur.reset();
    // why should call reset() here ? if you don`t, the ref count of this fiber won`t be 0 cause the
    // why the raw_ptr won`t call destructor here after cur.reset()? cause the handle of this fiber is still hold by main fiber, and the handle always be a Sptr of Fiber

    raw_ptr->Yield();
    //todo log here
    assert(false);//should never reach here
}

auto Fiber::IsMainFiber_() const
    -> bool
{
    return stk_size_ == 0;
}

} //namespace FiberT