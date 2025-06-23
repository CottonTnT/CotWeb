#ifndef __SYLAR_MUTEX_GUARD__
#define __SYLAR_MUTEX_GUARD__

#include "noncopyable.h"
#include <pthread.h>
#include <semaphore.h>
#include <cstdint>
#include <mutex>
#include <assert.h>

#define MCHECK(ret) ({ decltype(ret) errnum = (ret);         \
                       assert(errnum == 0); (void) errnum; })

#define CCHECK(call)                    \
    do {                                \
        decltype(call) errnum = (call); \
        assert(errnum == 0);            \
        (void)errnum;                   \
    } while (0);

namespace Cot {

class Semaphore : NonCopyable {
public:
    /**
     * @brief 构造函数
     * @param[in] count 信号量值的大小
     */
    Semaphore(uint32_t count = 0);

    /**
     * @brief 析构函数
     */
    ~Semaphore();

    /**
     * @brief 获取信号量
     */
    void wait();

    /**
     * @brief 释放信号量
     */
    void notify();

private:
    sem_t m_semaphore;
};

/**
 * @brief 普通锁
 */
class Mutex : NonCopyOrMoveable {
public:
    /// 局部锁
    /**
     * @brief 构造函数
     */
    Mutex()
    {
        pthread_mutex_init(&m_mutex_, nullptr);
    }

    /**
     * @brief 析构函数
     */
    ~Mutex()
    {
        pthread_mutex_destroy(&m_mutex_);
    }

    /**
     * @brief 加锁
     */
    void lock()
    {
        pthread_mutex_lock(&m_mutex_);
    }

    /**
     * @brief 解锁
     */
    void unlock()
    {
        pthread_mutex_unlock(&m_mutex_);
    }

    auto getPthreadMutex()
        -> pthread_mutex_t*
    {
        return &m_mutex_;
    }

private:
    /// mutex
    pthread_mutex_t m_mutex_;
};

/**
 * @brief 自旋锁
 */
class SpinMutex : NonCopyOrMoveable {
public:
    /**
     * @brief 构造函数
     */
    SpinMutex()
    {
        pthread_spin_init(&m_mutex, 0);
    }

    /**
     * @brief 析构函数
     */
    ~SpinMutex()
    {
        pthread_spin_destroy(&m_mutex);
    }

    /**
     * @brief 上锁
     */
    void lock()
    {
        pthread_spin_lock(&m_mutex);
    }

    /**
     * @brief 解锁
     */
    void unlock()
    {
        pthread_spin_unlock(&m_mutex);
    }

private:
    /// 自旋锁
    pthread_spinlock_t m_mutex;
};

class RWMutex : NonCopyOrMoveable {
public:
    RWMutex()
    {
        pthread_rwlock_init(&m_lock, nullptr);
    }

    void rdlock()
    {
        pthread_rwlock_rdlock(&m_lock);
    }

    void wrlock()
    {
        pthread_rwlock_wrlock(&m_lock);
    }

    void unlock()
    {
        pthread_rwlock_unlock(&m_lock);
    }

    ~RWMutex()
    {
        pthread_rwlock_destroy(&m_lock);
    }

private:
    pthread_rwlock_t m_lock;
};

template <typename Mutex, typename LockTag = void>
class LockGuard : NonCopyOrMoveable {
public:
    LockGuard(Mutex& mutex)
        : m_mutex(mutex)
    {
        m_mutex.lock();
    }
    ~LockGuard()
    {
        m_mutex.unlock();
    }

private:
    Mutex& m_mutex;
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
        mutex.wrlock();
    }

    void _lockDispatch(RWMutex& mutex, ReadLockTag tag)
    {
        mutex.rdlock();
    }

} //namespace

template <typename LockTag>
class LockGuard<RWMutex, LockTag> {
public:
    explicit LockGuard(RWMutex& mutex)
        : m_mutex(mutex)
    {
        _lockDispatch(m_mutex, LockTag());
    }

    ~LockGuard()
    {
        _unlockDispatch(m_mutex, LockTag());
    }

private:
    RWMutex& m_mutex;
    bool m_locked = false;
};

} //namespace Cot

#endif