#include "mutex.h"

namespace Cot {
//todo::exception handler

Semaphore::Semaphore(uint32_t count)
{
    if (sem_init(&sem_, 0, count) != 0)
    {
        // throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore()
{
    sem_destroy(&sem_);
}

void Semaphore::Wait()
{
    if (sem_wait(&sem_) != 0)
    {
        // throw std::logic_error("sem_wait error");
    }
}

void Semaphore::Notify()
{
    if (sem_post(&sem_) != 0)
    {
        // throw std::logic_error("sem_post error");
    }
}
} //namespace Cot