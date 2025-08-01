
#include "util.hpp"
#include "envvar/env.h"
#include <algorithm>
#include "envvar/envutils.h"

namespace Env::Utils{


auto getEnvOr(std::string_view key, std::string default_value)
    -> std::string
{
    auto ret_or = UtilT::SyscallWrapper<nullptr>(getenv, key.data());
    if (ret_or.has_value())
    {
        return ret_or.value();
    }
    return default_value;
}

auto getAbsolutePath(std::string_view path)
    -> std::string
{
    if(path.empty())
        return "/";
    if(path[0] == '/')
        return std::string{path};

    auto cwd = EnvT::EnvMgr::GetInstance().GetCwd();

    auto ret_str = std::string{};
    ret_str.reserve(cwd.size() + path.size());

    auto st_pos = std::copy(cwd.begin(),  cwd.end(), ret_str.begin());
    std::copy(path.begin(), path.end(), st_pos);
    return ret_str;
}



} //namespace Env::Utils
