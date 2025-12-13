#include "logger/AppenderProxy.hpp"
#include "logger/AsyncLogger.h"
#include "logger/LogAppender.h"
#include "logger/Logger.h"
#include <print>
#include <random>

/**
 * @brief 生成一个指定范围 [min, max] 内的随机整数。
 * * @param min 范围最小值（包含）。
 * @param max 范围最大值（包含）。
 * @return int 范围内的随机整数。
 */
int generate_random_int(int min, int max) {
    // 1. 播种 (Seeding)
    // 使用 std::random_device 作为种子源，提供硬件级的不确定性，
    // 确保每次程序运行的序列都不同。
    // 如果 random_device 不可用或性能要求高，可以使用时间作为备用。
    std::random_device rd;
    
    // 2. 随机数引擎 (Engine)
    // std::mt19937 是一个高质量、广泛使用的伪随机数生成器。
    // 使用 rd() 的结果作为引擎的初始种子。
    std::mt19937 generator(rd());

    // 3. 分布器 (Distribution)
    // std::uniform_int_distribution 用于在指定范围内产生均匀分布的整数。
    // 注意：范围 [min, max] 是包含 min 和 max 的。
    std::uniform_int_distribution<int> distribution(min, max);

    // 4. 生成随机数
    return distribution(generator);
}
auto main()
    -> int
{
    int lower_bound = 1;
    int upper_bound = 10000;
    auto logger = std::make_shared<Logger>();
    logger->addAppender(std::make_shared<AppenderProxy<RollingFileAppender>>("logfile.txt", 1_kb));
    LogFMTLevel(*logger, LogLevel::DEBUG, "test log fmt level: {}, {}, {}", generate_random_int(lower_bound,  upper_bound), generate_random_int(lower_bound,  upper_bound), "hello");
    LogFMTLevel(*logger, LogLevel::DEBUG, "test log fmt level: {}, {}, {}", generate_random_int(lower_bound,  upper_bound), generate_random_int(lower_bound,  upper_bound), "hello");
    LogFMTLevel(*logger, LogLevel::DEBUG, "test log fmt level: {}, {}, {}", generate_random_int(lower_bound,  upper_bound), generate_random_int(lower_bound,  upper_bound), 2, "hello");

    auto async_logger = std::make_shared<AsyncLogger>();
    async_logger->addAppender(std::make_shared<AppenderProxy<RollingFileAppender>>("async_logfile.txt", 1_kb));

    LogFMTLevel(*async_logger, LogLevel::DEBUG, "test log fmt level: {}, {}, {}", generate_random_int(lower_bound,  upper_bound), generate_random_int(lower_bound,  upper_bound), "hello");
    LogFMTLevel(*async_logger, LogLevel::DEBUG, "test log fmt level: {}, {}, {}", generate_random_int(lower_bound,  upper_bound), generate_random_int(lower_bound,  upper_bound), "hello");
    LogFMTLevel(*async_logger, LogLevel::DEBUG, "test log fmt level: {}, {}, {}", generate_random_int(lower_bound,  upper_bound), generate_random_int(lower_bound,  upper_bound), 2, "hello");
    return 0;
}