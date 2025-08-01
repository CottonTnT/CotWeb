#pragma once

#include "mutex.h"
#include "singleton.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <span>

namespace EnvT{


class Env{
public:
    using MutexType = Cot::RWMutex;
/**
  * @brief 初始化，包括记录程序名称与路径，解析命令行选项和参数
  * @details 命令行选项全部以-开头，后面跟可选参数，选项与参数构成key-value结构，被存储到程序的自定义环境变量中，
  * 如果只有key没有value，那么value为空字符串
  * @param[in] argc main函数传入
  * @param[in] argv main函数传入
  * @return  是否成功
*/
    auto Init(std::span<char*> args)
        -> bool;

    /**
     * @brief add self-define env variable
     */
    auto Add(std::string_view key, std::string_view val)
        -> bool;

    auto AddOrAssign(std::string_view key, std::string_view val)
        -> void;

    auto Contains(std::string_view key)
        -> bool;

    void Del(std::string_view key);

    auto GetOr(std::string_view key,std::string_view default_value = "")
        -> std::string;

    auto GetCwd() const&
        -> std::string_view;

    auto GetCwd() &&
        -> std::string;
private:
    Cot::RWMutex mtx_;
    std::unordered_map<std::string, std::string> env_vars_;
    std::string exe_fullpath_;
    std::string_view program_name_;
    std::string_view cwd_;
}; 

using EnvMgr = Cot::Singleton<Env>;

}//namespace EnvT