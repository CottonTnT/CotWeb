#ifndef UTIL_H
#define UTIL_H

#include <bits/types.h>
#include <cstdint>
#include <functional>
#include <string_view>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

namespace UtilT {

auto GetElapseMs()
    -> uint64_t;

auto GetThreadId()
    -> pid_t;

auto GetFiberId()
    -> pid_t;

/**
 * @brief 获取线程名称，参考pthread_getname_np(3)
 */
auto GetThreadName()
    -> std::string;

/**
 * @brief 获取当前的调用栈
 * @param[out] bt 保存调用栈
 * @param[in] size 最多返回层数
 * @param[in] skip 跳过栈顶的层数
 */
void Backtrace(std::vector<std::string>& bt,
               std::uint32_t size = 64,
               std::uint32_t skip = 1);

/**
 * @brief 获取当前栈信息的字符串
 * @param[in] size 栈的最大层数
 * @param[in] skip 跳过栈顶的层数
 * @param[in] prefix 栈信息前输出的内容
 */
auto BacktraceToString(std::uint32_t size = 64, std::uint32_t skip = 2, const std::string& prefix = "")
    -> std::string;

struct CallGuard
{
    using Callback = std::function<void()>;

    CallGuard(const CallGuard&)            = delete;
    CallGuard(CallGuard&&)                 = delete;
    CallGuard& operator=(const CallGuard&) = delete;
    CallGuard& operator=(CallGuard&&)      = delete;
    explicit CallGuard(Callback cb);
    ~CallGuard();

private:
    Callback call_;
};

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
    for(auto c : str) 
        hash_value = (hash_value ^ (c - '\0')) * prime;
    return hash_value;
    // return not str.empty() ? ((str[0] -'\0') ^ cHashString(str.substr(1))) * prime: base;
}

/**
 * @brief trasparent hash to gain performance in STL
 */
class Hasher {
public:
    using is_transparent = void;
    auto operator()(std::string_view sv) const
         -> size_t
    {
        return std::hash<std::string_view>{}(sv);
    }
};

} //namespace UtilT
#endif