#include "mutex.h"
#include "common.h"
#include "singleton.hpp"

#include  <string_view>
#include <unordered_map>

namespace LogT{
class Logger;

class LoggerManager{
public:
    using MutexType = Cot::SpinMutex;
    LoggerManager();

    void Init();

    /**
     * @brief Get or create a logger with given logger_name 
     * @todo make the new logger inheit config from the root logger
     */
    auto GetLogger(std::string_view logger_name)
        -> Sptr<Logger>;

    auto GetRoot()
        -> Sptr<Logger>{return root_;}
private:
    mutable MutexType mtx_;
    Sptr<Logger> root_;
    std::unordered_map<std::string, Sptr<Logger>> loggers_;
};

using LoggerMgr = Cot::Singleton<LoggerManager>;

} //namespace LogT