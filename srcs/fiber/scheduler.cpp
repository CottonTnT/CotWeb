
#include "common/fiber.h"
#include "common/curthread.h"
#include "common/mutex.h"
#include "common/thread.h"
#include "common/fiberstate.h"
#include "common/fiberutil.h"
#include "fiber/scheduler.h"
#include "fiber/scheduletask.h"
#include "logger/logger.h"
#include "logger/loggermanager.h"
#include <ctime>
#include <functional>
#include <memory>
#include <sys/select.h>

static auto s_Logger = GET_ROOT_LOGGER();

namespace CurThr {

using namespace FiberT;

/// 当前线程的调度器，同一个调度器下的所有线程指同同一个调度器实例
static thread_local auto* s_Scheduler = (Scheduler*) {nullptr};

/// 当前线程的调度协程，每个线程都独有一份，包括caller线程

auto GetScheduler()
    -> Scheduler*
{
    return s_Scheduler;
}

auto SetScheduler(Scheduler* sche)
    -> void
{
    s_Scheduler = sche;
}

} // namespace CurThr

namespace FiberT {

Scheduler::Scheduler(std::uint32_t max_worker_thread_num,
                     bool use_root_thread,
                     std::string name)
    : max_worker_thread_num_ {max_worker_thread_num}
    , name_ {std::move(name)}
    , root_thread_id_ {CurThr::GetId()}
    , use_root_thread_ {use_root_thread}
{
#ifndef NO_DEBUG
    LOG_DEBUG(s_Logger) << "Schduler:Schduler()";
#endif

    if (use_root_thread_)
    {
        LOG_DEBUG(s_Logger) << "Schduler:Schduler()\t create the root thread schedule fiber";

        root_thread_schedule_fiber_ = Fiber::CreateFiber(
            [scheduler = this]()
                -> void {
                auto main_fiber = CurThr::GetMainFiber();
#ifndef NO_DEBUG

                LOG_DEBUG(s_Logger) << "before enter Schduler::Run()";
#endif
                CurThr::SetMainFiber(scheduler->root_thread_schedule_fiber_);
                scheduler->Run();
                CurThr::SetMainFiber(main_fiber);
#ifndef NO_DEBUG
                LOG_DEBUG(s_Logger) << "after Schduler::Run(), go back to root thread main fiber";
#endif
        });
        CurThr::SetName(name_);
        CurThr::SetScheduler(this);

        thread_ids_.push_back(root_thread_id_);
    }
}

Scheduler::~Scheduler()
{

#ifndef NO_DEBUG
    LOG_DEBUG(s_Logger) << "Schduler:~Schduler()";
#endif
    assert(stopping_);

    if (CurThr::GetScheduler() == this) // 额，感觉这里有点多余
    {
        CurThr::s_Scheduler = nullptr;
    }
}

auto Scheduler::Start()
    -> void
{

#ifndef NO_DEBUG
    LOG_DEBUG(s_Logger) << "Scheduler::start()";
#endif

    // todo don`t know why lock here
    auto _ = LockGuard<Mutex>(mtx_);

    if (stopping_)
    {
        LOG_ERROR(s_Logger) << "schduler is stopped";
        return;
    }

    if (not threads_.empty())
    {
        LOG_ERROR(s_Logger) << "this sheduler start already";
        return;
    }

    auto actual_worker_thread_num = max_worker_thread_num_ - static_cast<std::uint32_t>(use_root_thread_);

    threads_.resize(actual_worker_thread_num);
    for (auto i = 0U; i < actual_worker_thread_num; i++)
    {
        // 线程的执行函数该Scheduler的run方法
        // new Thread()方法内部会创建一个线程执行
        threads_[i] = std::make_shared<Thr::Thread>(
            [scheduler = this]()
                -> void {
                scheduler->Run();
            },
            name_ + "_" + std::to_string(i));
        thread_ids_.push_back(threads_[i]->GetId());
    }
}

auto Scheduler::GetName() const& -> std::string
{
    return name_;
}

auto Scheduler::Run()
    -> void
{

// 1. 初始化运行环境
#ifndef NO_DEBUG
    LOG_DEBUG(s_Logger) << std::format("Scheduler::Run(): run in thread '{}:{}'",
                                       CurThr::GetName(),
                                       CurThr::GetId());
#endif

    CurThr::SetScheduler(this); // 设置当前线程的调度器实例
                                // 同一个调度器开启的线程的调度器线程的实例相同

    auto idle_fiber = Fiber::CreateFiber([scheduler = this]() -> void { scheduler->Idle(); }); // for non root thread, the main fiber will be created here;

    auto func_wrapper_fiber = Sptr<Fiber> {};
    // talk about: why not put the func_wrapper_fiber into the while block below?
    // for reuse the stack

    while (true)
    {
        auto task = ScheduleTask {};

        auto tickle_me = false; // 标记是否需要唤醒其他线程
        {                       // 2. 临界区：安全获取可执行协程
            auto _ = LockGuard<MutexType> {mtx_};

            auto it = tasks_.begin();

            while (it != tasks_.end())
            {
                if (auto onwner_id = it->GetOwerThreadId();
                    onwner_id != ScheduleTask::c_NonOwner
                    and onwner_id != CurThr::GetId())
                {
                    ++it;
                    tickle_me = true;
                    continue;
                }
#ifndef NO_ASSERT
                assert(not it->Empty());
#endif
                // todo: figure out the correctness of words below
                //  找到一个未指定线程，或是属于当前线程的任务
                //  [BUG FIX]: hook IO相关的系统调用时，在检测到IO未就绪的情况下，
                //  会先添加对应的读写事件，再yield当前协程，等IO就绪后再resume当前协程
                //  多线程高并发情境下，有可能发生刚添加事件就被触发的情况，
                //  如果此时当前协程还未来得及yield，则这里就有可能出现协程状态仍
                //  为RUNNING的情况
                //  这里简单地跳过这种情况，以损失一点性能为代价，否则整个协程框架都要大改
                //  if (it->IsFiber() && it->GetFiber()->GetState() == FiberState::RUNNING)
                //  {
                //      ++it;
                //      continue;
                //  }

                // 提取可执行协程
                task = *it;
                tasks_.erase(it++);     // 出队
                ++active_thread_count_; // 当前线程变为活跃
                break;
            }
            // 当前线程拿完一个任务后，发现任务队列还有剩余，那么tickle一下其他线程
            // 即使有可能剩余的任务都是本线程的~
            tickle_me |= (it != tasks_.end());
        }

        if (tickle_me)
        {
            Tickle(); // wake up other threads to work
        }
        if (task.Empty())
        {

#ifndef NO_DEBUG
            LOG_DEBUG(s_Logger) << std::format("Scheduler::Run(): no task founded");
#endif
            // 如果调度器没有调度任务，那么idle协程会不停地resume/yield，不会结束，如果idle协程结束了，那一定是调度器停止了
            if (idle_fiber->GetState() == FiberState::TERM)
            {
                LOG_INFO(s_Logger) << "idle fiber term";
                break;
            }
            ++idle_thread_count_;

            CurThr::Resume(idle_fiber);
            --idle_thread_count_;
            continue;
            // todo: how to deal with other state of idle
        }

#ifndef NO_DEBUG
        LOG_DEBUG(s_Logger) << std::format("Scheduler::Run(): find a task, owerId{}",
                                           task.GetOwerThreadId());
#endif

        // 4. 执行协程或函数
        if (task.IsFiber())
        {

            auto fiber = task.GetFiber();

#ifndef NO_DEBUG
            LOG_DEBUG(s_Logger) << std::format("Scheduler::Run(): task is a fiber, Fiber {} : {}",
                                               fiber->GetId(), FiberStateToString(fiber->GetState()));
#endif
            switch (fiber->GetState())
            {
                case FiberState::UNUSED:
                case FiberState::READY:
#ifndef NO_DEBUG
            LOG_DEBUG(s_Logger) << std::format("Scheduler::Run(): start to run fiber{} : {}",
                                               fiber->GetId(), FiberStateToString(fiber->GetState()));
#endif
                    CurThr::Resume(fiber);

#ifndef NO_DEBUG
            LOG_DEBUG(s_Logger) << std::format("Scheduler::Run(): back from  fiber{} : {} ",
                                               fiber->GetId(), FiberStateToString(fiber->GetState()));
#endif


                    --active_thread_count_;
                    break;
                default: // for other state, wrong here
                    LOG_ERROR(s_Logger) << std::format("run(): select the wrong fiber {}:{}", fiber->GetId(), FiberStateToString(fiber->GetState()));
                    assert(false);
            }
            switch (fiber->GetState())
            {
                case FiberState::READY:
                    Schedule(fiber);
                    --active_thread_count_;
                    break;
                case FiberState::HOLD:
                case FiberState::EXCEPT:
                case FiberState::TERM:
                    // todo::don`t know how to do
                    --active_thread_count_;
                    break;
                default: // for other state, wrong here
                    LOG_ERROR(s_Logger) << std::format("run(): after resume :the wrong fiber {}:{}", fiber->GetId(), FiberStateToString(fiber->GetState()));
                    assert(false);
            }
        }
        else if (task.IsFunc())
        {

            auto func = task.GetFunc();

            if (func_wrapper_fiber == nullptr)
            {
                func_wrapper_fiber = Fiber::CreateFiber(func);
            }
            else
            {
                func_wrapper_fiber->Reset(func);
            }

#ifndef NO_DEBUG
            LOG_DEBUG(s_Logger) << std::format("Scheduler::Run(): wrapped by fiber{} : {} and start to run",
                                               func_wrapper_fiber->GetId(), FiberStateToString(func_wrapper_fiber->GetState()));
#endif
            CurThr::Resume(func_wrapper_fiber);

#ifndef NO_DEBUG
            LOG_DEBUG(s_Logger) << std::format("Scheduler::Run(): back from wrapper fiber {} : {}",
                                               func_wrapper_fiber->GetId(), FiberStateToString(func_wrapper_fiber->GetState()));
#endif
            --active_thread_count_;

            switch (func_wrapper_fiber->GetState())
            {
                case FiberState::READY:
                    Schedule(func_wrapper_fiber);
                    func_wrapper_fiber.reset();
                    break;
                case FiberState::HOLD: // 挂起等待下次调度?
                    // todo:
                    break;
                case FiberState::EXCEPT:
                case FiberState::TERM:
                    func_wrapper_fiber->Reset(nullptr);
                    // todo::
                    break;
                default: // for other state, wrong here
                    LOG_ERROR(s_Logger) << std::format("run(): after resume :the wrong fiber {}:{}", func_wrapper_fiber->GetId(), FiberStateToString(func_wrapper_fiber->GetState()));
                    assert(false);
            }
        }
    }
}

auto Scheduler::Stop()
    -> void
{
#ifndef NO_DEBUG
    LOG_DEBUG(s_Logger) << "Schduler::Stop()";
#endif

    assert(IsRootThread_(CurThr::GetId())); // only root thread can call stop()

    // todo loghere
    {
        auto _ = LockGuard<MutexType>(mtx_);
        if (stopping_)
            return;
        stopping_ = true;
    }

    for (std::size_t i = 0; i < max_worker_thread_num_; ++i)
    {
        Tickle();
    }

    if (use_root_thread_)
    {

#ifndef NO_DEBUG
        LOG_DEBUG(s_Logger) << "root thread schedule fiber start";
#endif
        CurThr::Resume(root_thread_schedule_fiber_);
#ifndef NO_DEBUG
        LOG_DEBUG(s_Logger) << "root thread schedule Fiber end";
#endif
    }

    std::vector<Sptr<Thr::Thread>> thrs;
    {
        auto _ = LockGuard<MutexType>(mtx_);
        thrs.swap(threads_);
    }
    for (auto& i : thrs)
    {
        i->Join();
    }
}

auto Scheduler::Stopped()
    -> bool
{
    auto _ = LockGuard<MutexType> {mtx_};
    return stopping_ and tasks_.empty() and active_thread_count_ == 0;
}

auto Scheduler::Tickle()
    -> void
{
    LOG_INFO(s_Logger) << "tickle";
}

// 假设当线程A没有任务可以做，且整个协程调度还没结束（就是说虽然线程A没任务了，但是其他的线程还有任务正在执行，调度还没结束），此时会让线程A执行idle协程（也是一个子协程），idle协程内部循环判断协程调度是否停止。

// 如果未停止，则将idle协程置为HOLD状态，让出执行权，继续运行run方法内的while循环，从任务队列取任务。（属于忙等状态，CPU占用率爆炸）

// 如果已经停止，则idle协程执行完毕，将idle协程状态置为TERM，协程调度结束。
auto Scheduler::Idle()
    -> void
{
#ifndef NO_DEBUG
    LOG_DEBUG(s_Logger) << "Scheduler::idle() ~~~~~~~~~~~~~";
#endif

    while (not Stopped())
    {
        // Fiber::YieldToHold();
        CurThr::YieldToReady();
    }
#ifndef NO_DEBUG
    LOG_DEBUG(s_Logger) << "Scheduler::idle() : over";
#endif
    // 当stopping()为true，没有任务要执行了，执行完idle，状态置为TERM
}

} // namespace FiberT