#pragma once

//just for clangd
#define __cpp_lib_expected 1

#include <expected>
#include <iostream>


namespace UtilT {

template <auto c_X,auto... c_XS>
inline constexpr bool c_IsOneOfValues = ((c_X == c_XS) or ...);

template <auto... c_ErrorFlag, typename Scall, typename... Args>
requires(std::is_invocable_v<Scall, Args...>)
auto SyscallWrapper(Scall scall, Args... args)
    -> std::expected<std::invoke_result_t<Scall, Args...>, std::string>
{
    auto ret = scall(args...);
    if(( (ret == c_ErrorFlag) or ... ))
    {
        auto ec = std::make_error_code(std::errc{errno});
        return std::unexpected<std::string>{ec.message()};
    }
    return ret;
}




}