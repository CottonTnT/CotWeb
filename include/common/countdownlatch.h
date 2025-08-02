#ifndef COUNTDOWNLATCH_H
#define COUNTDOWNLATCH_H

#include "mutex.h"
#include "condition.h"

namespace Cot {

class CountDownLatch{

public:
    explicit CountDownLatch(int count);

    void Wait();

    void CountDown();

    auto GetCount() const
        -> int;

private:
    mutable Mutex mtx_;
    Condition cond_;
    int count_;
};

} //namespace Cot

#endif