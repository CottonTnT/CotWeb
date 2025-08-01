#include "envvar/env.h"
#include "util.hpp"
#include "logger/loggermanager.h"
#include "logger/logger.h"


#include <exception>
#include <format>
#include <unistd.h>


namespace EnvT{


constexpr int c_BufSize = 1024;


static auto s_Logger = GET_LOGGER_BY_NAME("system");


auto Env::Init(std::span<char*> cmd_args)
    -> bool
{

    auto link_path = std::format("/proc/{}/exe", getpid()); // /proc/%d/exe is a symlink to the running process, and the call of getpid is always successful

    auto fullpath = std::string(c_BufSize, '\0');

    auto ret_or = UtilT::SyscallWrapper<-1>(readlink, link_path.c_str(), fullpath.data(), fullpath.size());

    if (not ret_or.has_value())
    {
        COT_LOG_FATAL(s_Logger) << ret_or.error(); 
        std::terminate();
    }

    exe_fullpath_ = std::move(fullpath);
    
    auto last_slash_pos = exe_fullpath_.find_last_of('/');

    cwd_ = std::string_view{exe_fullpath_.c_str(), exe_fullpath_.c_str() + last_slash_pos};

    program_name_ = std::string_view{cmd_args[0]};

    //todo:parse cmd args, wait for config module and argparse lib

    return true;
}

auto Env::Add(std::string_view key, std::string_view val)
    -> bool
{
    auto lk_guard = Cot::LockGuard<MutexType, Cot::WriteLockTag>{mtx_};
    if (auto ok_or = env_vars_.try_emplace(std::string(key), std::string(val)); not ok_or.second)
    {
        COT_LOG_ERROR(s_Logger) << "'Env::Add' the key is already exist!";
        return false;
    }
    return true;
}

auto Env::AddOrAssign(std::string_view key, std::string_view val)
    -> void
{
    auto lk_guard = Cot::LockGuard<MutexType, Cot::WriteLockTag>{mtx_};
    env_vars_.emplace(std::string(key), std::string(val));
}

auto Env::Contains(std::string_view key)
    -> bool
{
    auto lk_guard = Cot::LockGuard<MutexType, Cot::ReadLockTag>{mtx_};
    return env_vars_.contains(std::string(key));
}

void Env::Del(std::string_view key)
{
    auto lk_guard = Cot::LockGuard<MutexType, Cot::WriteLockTag>{mtx_};
    env_vars_.erase(std::string(key));
}

auto Env::GetCwd() const&
    ->  std::string_view
{
    return cwd_;
}

auto Env::GetCwd() &&
    -> std::string
{
    return  std::string{cwd_};
}


} //namespace EnvT