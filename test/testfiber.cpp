#include "fiber/fiber.h"
#include "logger/logger.h"
#include "logger/loggermanager.h"


Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER();

void run_in_fiber() {

    LOG_INFO(g_logger) << "run_in_fiber begin";
    // Yield和YieldToHold的区别是，前者是成员方法，后者为静态方法，后者会获取当前协程，然后调用Yield
    CurThr::YieldToReady();
    LOG_INFO(g_logger) << "run_in_fiber midle";
    CurThr::YieldToReady();
    LOG_INFO(g_logger) << "run_in_fiber end";
    // -> MainFunc - Yield()  // --------------------> 6
}

using namespace FiberT;

auto main()
    -> int{
    LOG_INFO(g_logger) << "main begin start1";
    {
        LOG_INFO(g_logger) << "main begin";

        auto fiber = Fiber::CreateFiber(run_in_fiber);

        CurThr::Resume(fiber);
        LOG_INFO(g_logger) << "main after resume1";
        CurThr::Resume(fiber);
        LOG_INFO(g_logger) << "main after resume2";
        CurThr::Resume(fiber);
        LOG_INFO(g_logger) << "main after resume3";
    }
    LOG_INFO(g_logger) << "main after end2"; // 7
    return 0;
}