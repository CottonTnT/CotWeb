#pragma once

#include <cstdint>
#include <optional>
#include <unistd.h>
#include <string>

#include "alias.h"
namespace CurThr{

    auto GetId()
        -> pid_t;
    
    auto GetName()
        -> std::string;

    auto SetName(std::string name)
        -> void;

    // auto GetCurThrHandle()
    //     -> Sptr<Thr::Thread>;

    // auto SetCurThrName(std::string name)
    //     -> void;
    
} //namespace CurThr

namespace FiberT {
class Fiber;
}
namespace CurThr {

using namespace FiberT;
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


auto Resume(Sptr<Fiber> fiber)
    -> void;


auto SetMainFiber(Sptr<Fiber> fiber)
    -> void;

auto GetMainFiber()
    -> Sptr<Fiber>;

auto GetRawMainFiber()
    -> Fiber*;

// void SetRunningFiber(Fiber* f);
void SetRunningFiber(Sptr<Fiber> f);

/**
 * @brief 返回当前线程正在执行的协程
 */
auto GetRunningFiber()
    -> Sptr<Fiber>;


auto GetRawRunningFiber()
    -> Fiber*;
} // namespace CurThr