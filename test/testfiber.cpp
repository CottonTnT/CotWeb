#include "common/fiber.h"
#include "logger/logger.h"
#include "logger/loggermanager.h"
#include <exception>


Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER();

void run_in_fiber() {

    LOG_INFO(g_logger) << "run_in_fiber begin";
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
        try
        {
            CurThr::Resume(fiber);
        } catch (std::exception& ex)
        {
            LOG_INFO(g_logger) << ex.what();
            LOG_INFO(g_logger) << "main after resume4";
        }
    }
    LOG_INFO(g_logger) << "main after end2"; // 7
    return 0;
}