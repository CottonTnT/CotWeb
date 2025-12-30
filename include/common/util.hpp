#pragma once

// just for clangd
#include "net/Callbacks.h"
#include <concepts>
#include <iterator>
#include <memory>
#include <utility>
#define __cpp_lib_expected 1

#include <cxxabi.h>
#include <expected>
#include <system_error>
#include <iostream>

namespace UtilT {

template <auto c_X, auto... c_XS>
inline constexpr bool c_IsOneOfValues = ((c_X == c_XS) or ...);

template <auto... c_ErrorFlag, typename Scall, typename... Args>
    requires(std::is_invocable_v<Scall, Args...>)
auto SyscallWrapper(Scall scall, Args... args)
    -> std::expected<std::invoke_result_t<Scall, Args...>, std::string>
{
    auto ret = scall(std::forward<Args>(args)...);
    if (((ret == c_ErrorFlag) or ...))
    {
        auto ec = std::make_error_code(std::errc {errno});
        return std::unexpected<std::string> {ec.message()};
    }
    return ret;
}

/**
 * @brief get the string of Type Name
 */
template <typename T>
auto GetTypeName()
    -> std::string_view
{
    static auto s_TypeName = std::string_view {abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr)};
    return s_TypeName;
}

using Hash_t = std::uint64_t;

inline constexpr Hash_t c_HashBase  = 0xCBF29CE484222325ULL;
inline constexpr Hash_t c_HashPrime = 0x100000001B3ULL;

/**
 * @brief FNV-1a hash
 *
 */
constexpr inline auto cHashString(std::string_view str, Hash_t base = c_HashBase, Hash_t prime = c_HashPrime)
    -> Hash_t
{

    Hash_t hash_value = 0;
    for (auto c : str)
        hash_value = (hash_value ^ (c - '\0')) * prime;
    return hash_value;
    // return not str.empty() ? ((str[0] -'\0') ^ cHashString(str.substr(1))) * prime: base;
}

template <typename T>
concept PointerLike = requires(T ptr) {
    { ptr == nullptr } -> std::convertible_to<bool>;
};

template <PointerLike T>
auto requiresNonNull(T&& ptr, const std::string& message = "Critical Error: Null pointer detected.")
    -> decltype(auto)
{
    if (ptr == nullptr) {
        std::cerr << "\033[1;31m[FATAL]\033[0m " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
    return std::forward<T>(ptr);
}

}