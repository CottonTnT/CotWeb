#ifndef CONDITION_H
#define CONDITION_H

#include <pthread.h>

#include "noncopyable.h"
#include "mutex.h"

namespace Cot {

class Condition {

public:
    using MutexType = Cot::Mutex;
    Condition()
    {
        CCHECK(pthread_cond_init(&cond_, nullptr));
    }

    Condition(const Condition&)            = delete;
    Condition(Condition&&)                 = delete;
    auto operator=(const Condition&)
        ->Condition&    = delete;
    auto operator=(Condition&&)
        ->Condition&   = delete;

    ~Condition()
    {
        MCHECK(pthread_cond_destroy(&cond_));
    }

    void Wait(MutexType& mutex)
    {
        pthread_cond_wait(&cond_, mutex.GetPthreadMutex());
    }

    void Notify()
    {
        CCHECK(pthread_cond_signal(&cond_));
    }

    void NotifyAll()
    {
        MCHECK(pthread_cond_broadcast(&cond_));
    }

private:
    pthread_cond_t cond_;
};

} //namespace Cot
#endif