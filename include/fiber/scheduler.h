#pragma  once

#include "common/mutex.h"
#include "common/alias.h"
#include "common/thread.h"
#include "common/fiber.h"
#include "scheduletask.h"

#include <atomic>
#include <list>


namespace FiberT{

using namespace Cot;


/**
 * @brief 协程调度器
 * @details 封装的是N-M的协程调度器
 *          内部有一个线程池,支持协程在线程池里面切换
 */
class Scheduler{
public:
    using MutexType = Mutex;


    /**
     * @brief 构造函数
     * @param[in] threads 线程数量
     * @param[in] use_caller 是否使用当前调用线程
     * @param[in] name 协程调度器名称
     */
    explicit Scheduler(std::uint32_t max_worker_thread_num = 1,
                       bool use_root_thread                = true,
                       std::string name = "");

    Scheduler(const Scheduler&)            = delete;
    Scheduler(Scheduler&&)                 = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler& operator=(Scheduler&&)      = delete;

    virtual ~Scheduler();

    [[nodiscard]] auto GetName() const&
        -> std::string;

    /**
     * @brief 主要初始化调度线程池，如果只使用caller线程进行调度，那这个方法啥也不做
     */
    auto Start()
        -> void;

    /**
     * @brief stop the scheduler
     * @attention when use_root_thread == true, only call stop will start the scheduler fiber of root thread
     */
    auto Stop()
        -> void;


    /**
     * @brief 添加调度任务
     * @tparam FiberOrCb 调度任务类型，可以是协程对象或函数指针
     * @param[] fc 协程对象或指针
     * @param[] thread 指定运行该任务的线程号，-1表示任意线程
     */
    template<class FiberOrCb>
    requires IsScheduleTask<FiberOrCb>
    void Schedule(FiberOrCb fc, int thread_id = -1) {
        bool need_tickle = false;
        {
            auto _ = LockGuard<MutexType>{mtx_};
            need_tickle = ScheduleNoLock_(fc, thread_id);
        }

        if(need_tickle) {
            Tickle();
        }
    }

    template <class InputIterator>
    void Schedule(InputIterator begin, InputIterator end)
    {
        bool need_tickle = false;
        {
            auto _ = LockGuard<MutexType>{mtx_};
            while(begin != end) {
                need_tickle = ScheduleNoLock_(&*begin, -1) || need_tickle;
                ++begin;
            }
        }
        if(need_tickle) {
            Tickle();
        }
    }

protected:

    /**
     * @brief 通知协程调度器有任务了
     */
    virtual auto Tickle()
        -> void;

    /**
     * @brief 协程无任务可调度时执行idle协程
     */
    virtual auto Idle()
        -> void;

    /**
     * @brief 返回是否可以停止
     */
    virtual auto Stopped()
        -> bool;

    /**
     * @brief 协程调度函数
     */
    auto Run()
        -> void;

    /**
     * @brief 是否有空闲线程
     */
    auto HasIdleThreads()
        -> bool{ return idle_thread_count_ > 0;}
private:

    /**
     * @brief 添加调度任务，无锁
     * @tparam FiberOrCb 调度任务类型，可以是协程对象或函数指针
     * @param[] fc 协程对象或指针
     * @param[] thread 指定运行该任务的线程号，-1表示任意线程
     */

    template <class FiberOrCb>
    auto ScheduleNoLock_(FiberOrCb fc, int thread)
        -> bool{
        bool need_tickle = tasks_.empty();
        if(fc != nullptr) {
            tasks_.emplace_back(fc, thread);
        }
        return need_tickle;
    }

    [[nodiscard]] auto IsRootThread_(pid_t thread_id) const
        -> bool
    {
        return root_thread_id_ == thread_id;
    }

private:

    MutexType mtx_;

    std::uint32_t max_worker_thread_num_; //工作线程数量，不包含user_caller的主线程
    std::atomic<std::uint32_t> active_thread_count_{0}; //the number of worker thread, i.e. running task

    std::atomic<std::uint32_t> idle_thread_count_ {0};
    std::string name_; 

    std::vector<Sptr<Thr::Thread>> threads_;

    std::list<ScheduleTask> tasks_; // the queue of fibers waiting for executing

    Sptr<Fiber> root_thread_schedule_fiber_; // valid when use_caller = true, 调度器所在线程的调度协程
    pid_t root_thread_id_ = -1; // the id of root thread

    std::vector<int> thread_ids_;//线程池的线程id的数组

    bool stopping_ = false;
    bool use_root_thread_;
};

} //namespace FiberT

namespace CurThr{

using namespace FiberT;
auto GetScheduler()
    -> Scheduler*;

auto SetScheduler(Scheduler* sche)
    -> void;

} //namespace CurThr