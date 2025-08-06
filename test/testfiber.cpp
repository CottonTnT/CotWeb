#include "fiber/fiber.h"
#include "logger/logappender.h"
#include "logger/logger.h"
#include "logger/loggermanager.h"
#include "logger/loglevel.h"


Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER();

void run_in_fiber() {
    LOG_INFO(g_logger) << "run_in_fiber begin";
    // Yield和YieldToHold的区别是，前者是成员方法，后者为静态方法，后者会获取当前协程，然后调用Yield
    FiberT::Fiber::GetCurThrRunningFiber()->Yield();
    LOG_INFO(g_logger) << "run_in_fiber end";
    FiberT::Fiber::GetCurThrRunningFiber()->Yield();
    LOG_INFO(g_logger) << "will back to SubFiberMain";
    // -> MainFunc - Yield()  // --------------------> 6
}

using namespace FiberT;

auto main()
    -> int{
    LOG_INFO(g_logger) << "main begin -1";
    {
        FiberT::Fiber::GetCurThrRunningFiber(); // 设置主协程
        LOG_INFO(g_logger) << "main begin";

        auto fiber = Fiber::CreateSubFiber(run_in_fiber);

        fiber->Resume(); // --------------------------> 1
        LOG_INFO(g_logger) << "main after swapIn";
        fiber->Resume(); // --------------------------> 3
        LOG_INFO(g_logger) << "main after end";
        LOG_INFO(g_logger) << "gogogo";
        fiber->Resume(); // --------------------------> 5
    }
    LOG_INFO(g_logger) << "main after end2"; // 7
    return 0;
}