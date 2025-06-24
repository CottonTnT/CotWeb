#ifndef __COT_SINGLETON_GUARD__
#define __COT_SINGLETON_GUARD__
#include "common.h"

#include <memory>


namespace Cot {


template <typename T, typename Tag, typename TagIdx>
requires std::is_default_constructible_v<T>
auto GetInstanceX()
    -> T&
{
    static auto s_entity = T{};
    return s_entity;
}

template <typename T, typename Tag, typename TagIdx>
requires std::is_default_constructible_v<T>
auto GetInstanceSptr()
    -> Sptr<T>
{
    static auto s_entity_sptr = Sptr<T>(new T{});
    return s_entity_sptr;
}

/**
 * @brief 单例模式封装类
 * @details T 类型
 *          X 为了创造多个实例对应的Tag
 *          N 同一个Tag创造多个实例索引
 */
template <class T, class X = void, int N = 0>
class Singleton {
public:
    /**
     * @brief 返回单例裸指针
     */
    static T& GetInstance()
    requires std::is_default_constructible_v<T>
    {
        static auto s_entity = T();
        return s_entity;
        //return &GetInstanceX<T, X, N>();
    }

    template <typename...Args>
    requires std::is_constructible_v<T,  Args...>
    static T& GetInstance(Args&&... args)
    {
        static auto s_entity = T{std::forward<Args>(args)...};
        return s_entity;
        //return &GetInstanceX<T, X, N>();
    }
};

/**
 * @brief 单例模式智能指针封装类
 * @details T 类型
 *          X 为了创造多个实例对应的Tag
 *          N 同一个Tag创造多个实例索引
 */

template <class T, class X = void, int N = 0>
class SingletonPtr {
public:
    /**
     * @brief 返回单例智能指针
     * @todo 也许这里返回值可以改为std::weak_ptr,但两者开销差不多，也无所谓吧
     */
    static auto GetInstance()
        -> Sptr<T>
    requires std::is_default_constructible_v<T>
    {
        static Sptr<T> v(new T);
        return v;
        //return GetInstancePtr<T, X, N>();
    }

    template <typename...Args>
    requires std::is_constructible_v<T,  Args...>
    static auto GetInstance(Args&&... args)
        -> Sptr<T>
    {
        static auto s_entity = Sptr<T>(new T{std::forward<Args>(args)...});
        return s_entity;
        //return &GetInstanceX<T, X, N>();
    }
};

} //namespace Cot

#endif