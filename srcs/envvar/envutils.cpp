#include "util.hpp"
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



} //namespace Env::Utils
