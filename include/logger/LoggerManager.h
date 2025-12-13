#include "common/alias.h"
#include "common/singleton.hpp"
#include "common/util.h"

#include <functional>
#include <mutex>
#include  <string_view>
#include <unordered_map>

#define GET_ROOT_LOGGER() LoggerMgr::GetInstance().getRoot()

#define GET_LOGGER_BY_NAME(name) LoggerMgr::GetInstance().getLogger(name)

class Logger;

class LoggerManager{
public:
    LoggerManager();

    void init_();

    /**
     * @brief Get or create a logger with given logger_name 
     * @todo make the new logger inheit config from the root logger
     */
    auto getLogger(std::string_view logger_name)
        -> Sptr<Logger>;

    auto getRoot()
        -> Sptr<Logger>{return root_;}
private:
    mutable std::mutex mtx_;
    Sptr<Logger> root_;
    std::unordered_map<std::string, Sptr<Logger>, UtilT::Hasher, std::equal_to<>> loggers_;
};


using LoggerMgr = Cot::Singleton<LoggerManager>;

