#include "mutex.h"
#include "countdownlatch.h"

namespace Cot {

CountDownLatch::CountDownLatch(int count)
    : count_(count)
{
}

void CountDownLatch::Wait()
{
    mtx_.Lock();
    while (count_ > 0)
    {
        cond_.Wait(mtx_);
    }
}

void CountDownLatch::CountDown()
{
    auto _ = LockGuard<Mutex>(mtx_);
    count_--;
    if (count_ == 0)
    {
        cond_.NotifyAll();
    }
}

auto CountDownLatch::GetCount() const
    -> int
{
    auto lk = LockGuard<Mutex>(mtx_);
    return count_;
}

} //namespace Cot