#define pragma once

#include <concepts>
#include <utility>

namespace X {

template <typename T>
class Singleton {

private:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

protected:
    Singleton() = default;
    ~Singleton() = default;

public:
    template <typename... Args>
    // requires std::constructible_from<T, Args...>, 不能加约束，对于T 的构造函数要求public，傻呗
    static auto getInstance(Args&&... args)
        -> T&
    {
        static auto instance = T(std::forward<Args>(args)...);
        return instance;
    }
};

template <typename T>
class SingletonBean {
protected:
    // 只有 BaseFactory 和 Derived 可以访问 Key 的构造函数
    class BeanKey {
        friend class SingletonBean<T>;
        BeanKey() = default;
    };
    SingletonBean() = default;
    ~SingletonBean() = default;

public:
    // 禁用拷贝和移动，确保单例语义
    SingletonBean(const SingletonBean&) = delete;
    SingletonBean& operator=(const SingletonBean&) = delete;
    SingletonBean(SingletonBean&&) = delete;
    SingletonBean& operator=(SingletonBean&&) = delete;

    template <typename... Args>
        requires std::constructible_from<T, BeanKey, Args...>
    static auto getInstance(Args&&... args)
        -> T&
    {

        static auto derived = T(BeanKey { }, std::forward<Args>(args)...);
        return derived;
    }
};

} // namespace X

// class Integer : public X::Singleton<Integer> {
//     friend class X::Singleton<Integer>; // 允许 Singleton 访问私有构造函数
// private:
//     Integer(int value)
//         : value_(value)
//     {
//     }
//     ~Integer() = default;

//     int value_;
// };

// class Bean : public X::SingletonBean<Bean> {
// public:
//     // 使用 Key 构造函数，确保只能通过 SingletonBean 获取实例
//     Bean(typename X::SingletonBean<Bean>::Key, int value)
//         : value_(value)
//     {
//     }

//     int value_;
// };

// auto main() -> int
// {
//     auto& beanInstance1 = X::SingletonBean<Bean>::getInstance(42);
//     // 正确方式：通过父类工厂构造

//     auto& singleton = X::Singleton<Integer>::getInstance(42);

//     // auto* bean = Bean(typename X::SingletonBean<Bean>::Key { }, 42);

//     return 0;
// }
