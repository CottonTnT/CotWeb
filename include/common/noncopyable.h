#ifndef __SYLAR_NONCOPYABLE_GUARD__
#define __SYLAR_NONCOPYABLE_GUARD__

namespace Cot {

class NonCopyable {
protected:
    /**
     * @brief 默认构造函数
     */
    NonCopyable()  = default;
    ~NonCopyable() = default;

public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};

class NonCopyOrMoveable {
protected: 
    NonCopyOrMoveable() = default;
    ~NonCopyOrMoveable() = default;
public:
    NonCopyOrMoveable(const NonCopyOrMoveable&) = delete;
    NonCopyOrMoveable& operator=(const NonCopyOrMoveable&) = delete;
    NonCopyOrMoveable(NonCopyOrMoveable&&) = delete;
    NonCopyOrMoveable& operator=(NonCopyOrMoveable&&) = delete;
};

}

#endif