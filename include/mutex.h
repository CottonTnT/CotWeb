#ifndef __SYLAR_MUTEX_GUARD__
#define __SYLAR_MUTEX_GUARD__

#include <assert.h>
#include <cstdint>
#include <pthread.h>
#include <semaphore.h>
#include <type_traits>

#define MCHECK(ret) ({ decltype(ret) errnum = (ret);         \
                       assert(errnum == 0); (void) errnum; })

#define CCHECK(call)                    \
    do {                                \
        decltype(call) errnum = (call); \
        assert(errnum == 0);            \
        (void)errnum;                   \
    } while (0);

namespace Cot {

class Semaphore {
public:
    /**
     * @brief 构造函数
     * @param[in] count 信号量值的大小
     */
    explicit Semaphore(uint32_t count = 0);

    Semaphore(const Semaphore&)            = delete;
    Semaphore(Semaphore&&)                 = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore& operator=(Semaphore&&)      = delete;
    /**
     * @brief 析构函数
     */
    ~Semaphore();

    /**
     * @brief 获取信号量
     */
    void Wait();

    /**
     * @brief 释放信号量
     */
    void Notify();

private:
    sem_t sem_;
};

/**
 * @brief 普通锁
 */
class Mutex {
public:
    /**
     * @brief 构造函数
     */
    Mutex()
    {
        pthread_mutex_init(&mtx_, nullptr);
    }

    Mutex(const Mutex&)            = default;
    Mutex(Mutex&&)                 = delete;
    Mutex& operator=(const Mutex&) = default;
    Mutex& operator=(Mutex&&)      = delete;
    /**
     * @brief 析构函数
     */
    ~Mutex()
    {
        pthread_mutex_destroy(&mtx_);
    }

    /**
     * @brief 加锁
     */
    void Lock()
    {
        pthread_mutex_lock(&mtx_);
    }

    /**
     * @brief 解锁
     */
    void Unlock()
    {
        pthread_mutex_unlock(&mtx_);
    }

    auto GetPthreadMutex()
        -> pthread_mutex_t*
    {
        return &mtx_;
    }

private:
    /// mutex
    pthread_mutex_t mtx_;
};

/**
 * @brief 自旋锁
 */
class SpinMutex{
public:

    SpinMutex()
    {
        pthread_spin_init(&mtx_, 0);
    }

    SpinMutex(const SpinMutex&)            = delete;
    SpinMutex(SpinMutex&&)                 = delete;
    SpinMutex& operator=(const SpinMutex&) = delete;
    SpinMutex& operator=(SpinMutex&&)      = delete;
    /**
     * @brief 析构函数
     */
    ~SpinMutex()
    {
        pthread_spin_destroy(&mtx_);
    }

    /**
     * @brief 上锁
     */
    void Lock()
    {
        pthread_spin_lock(&mtx_);
    }

    /**
     * @brief 解锁
     */
    void Unlock()
    {
        pthread_spin_unlock(&mtx_);
    }

private:
    /// 自旋锁
    pthread_spinlock_t mtx_;
};

class RWMutex{
public:
    RWMutex()
    {
        pthread_rwlock_init(&lock_, nullptr);
    }

    RWMutex(const RWMutex&)            = default;
    RWMutex(RWMutex&&)                 = delete;
    RWMutex& operator=(const RWMutex&) = default;
    RWMutex& operator=(RWMutex&&)      = delete;
    void Rdlock()
    {
        pthread_rwlock_rdlock(&lock_);
    }

    void Wrlock()
    {
        pthread_rwlock_wrlock(&lock_);
    }

    void Unlock()
    {
        pthread_rwlock_unlock(&lock_);
    }

    ~RWMutex()
    {
        pthread_rwlock_destroy(&lock_);
    }

private:
    pthread_rwlock_t lock_;
};

template <typename Mutex, typename LockTag = void>
class LockGuard{
public:

    LockGuard(Mutex& mutex)
        : mtx_(mutex)
    {
        mtx_.lock();
    }

    LockGuard(const LockGuard&)            = delete;
    LockGuard(LockGuard&&)                 = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    LockGuard& operator=(LockGuard&&)      = delete;

    ~LockGuard()
    {
        mtx_.unlock();
    }

private:
    Mutex& mtx_;
};

struct ReadLockTag
{
};

struct WriteLockTag
{
};

namespace {

    template <typename MutexType>
    void _lockDispatch(MutexType& mutex, WriteLockTag)
    {
        mutex.lock();
    }
    template <typename MutexType>
    void _lockDispatch(MutexType& mutex, ReadLockTag tag)
    {
        mutex.lock();
    }

    template <typename MutexType>
    void _unlockDispatch(MutexType& mutex, WriteLockTag tag)
    {
        mutex.unlock();
    }

    template <typename MutexType>
    void _unlockDispatch(MutexType& mutex, ReadLockTag tag)
    {
        mutex.unlock();
    }

    void _lockDispatch(RWMutex& mutex, WriteLockTag tag)
    {
        mutex.Wrlock();
    }

    void _lockDispatch(RWMutex& mutex, ReadLockTag tag)
    {
        mutex.Rdlock();
    }

} //namespace

/**
 * @brief deprecated
 */
// template <typename LockTag>
// class LockGuard<RWMutex, LockTag> {
// public:
//     explicit LockGuard(RWMutex& mutex)
//         : m_mutex(mutex)
//     {
//         _lockDispatch(m_mutex, LockTag());
//     }

//     ~LockGuard()
//     {
//         _unlockDispatch(m_mutex, LockTag());
//     }

// private:
//     RWMutex& m_mutex;
//     bool m_locked = false;
// };

template <typename LockTag>
class LockGuard<RWMutex, LockTag> {
public:
    LockGuard(const LockGuard&)            = delete;
    LockGuard(LockGuard&&)                 = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    LockGuard& operator=(LockGuard&&)      = delete;

    explicit LockGuard(RWMutex& mutex)
        : mtx_(mutex)
    {
        if constexpr(std::is_same_v<LockTag,  ReadLockTag>)
            mtx_.Rdlock();
        else
            mtx_.Wrlock();
        locked_ = true;
    }

    ~LockGuard()
    {
        mtx_.Unlock();
    }

private:
    RWMutex& mtx_;
    bool locked_ = false;
};



} //namespace Cot

#endif