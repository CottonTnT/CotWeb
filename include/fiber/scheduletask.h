#pragma once

#include "common/alias.h"
#include <functional>
#include <sched.h>
#include <type_traits>
#include <utility>
#include <variant>

namespace FiberT{

class Fiber;
/**
 * @brief 协程/函数/线程组
 */

template <typename Callable>
concept IsScheduleTask =  std::is_same_v<Callable, Sptr<Fiber>> or std::is_convertible_v<Callable, std::function<void()>>;

class ScheduleTask {
private:
    using FB = Sptr<Fiber>;
    using Func = std::function<void()>;

public: 

    static inline constexpr pid_t c_NonOwner = -1;
    /**
     * @brief 构造函数
     * @param[in] f 协程
     * @param[in] thr 线程id
     */
    ScheduleTask(FB f, pid_t owner_id)
        : callback_ {std::in_place_type_t<FB> {}, f}
        , owner_thread_id_(owner_id)
        {}

    /**
     * @brief 构造函数
     * @param[in] f 协程执行函数
     * @param[in] thr 线程id
     */
    ScheduleTask(Func f, pid_t owner_id)
        : callback_ {std::in_place_type_t<Func> {}, std::move(f)}
        , owner_thread_id_(owner_id)
        {}

    /**
     * @brief 无参构造函数
     */
    ScheduleTask() = default;

    /**
     * @brief 重置数据
     */
    void Reset() {
        callback_ = FB{nullptr};
        owner_thread_id_ = c_NonOwner;
    }

    [[nodiscard]] auto GetOwerThreadId() const
        -> pid_t
    {
        return owner_thread_id_;
    }

    [[nodiscard]] auto Empty() const
        -> bool
    {
        return std::visit([](const auto& value) -> bool {
            return value == nullptr;
        }, callback_);
    }

    template <typename T>
    requires (std::is_same_v<T, FB> or std::is_same_v<T, Func>)
    [[nodiscard]] auto Is() const
        -> bool
    {
        return std::holds_alternative<T>(callback_);
    }


    [[nodiscard]]auto IsFiber() const
        -> bool
    {
        return Is<FB>();
    }

    [[nodiscard]]auto IsFunc() const
        -> bool
    {
        return Is<Func>();
    }

    [[nodiscard]]auto GetFiber() const
        -> const FB&
    {
        return std::get<FB>(callback_);
    }

    [[nodiscard]]auto GetFunc() const
        -> const Func&
    {
        return std::get<Func>(callback_);
    }

    [[nodiscard]]auto GetFiber() 
        ->  FB&
    {
        return std::get<FB>(callback_);
    }

    [[nodiscard]]auto GetFunc() 
        ->  Func&
    {
        return std::get<Func>(callback_);
    }

private:
    std::variant<FB, Func> callback_;
    /// 线程id
    pid_t owner_thread_id_ = c_NonOwner; //若不指定，则所有线程均可执行该任务
};

} //namespace FiberT