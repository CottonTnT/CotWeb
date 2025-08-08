#include "fiber/fiberutil.h"

namespace FiberT{
auto FiberStateToString(FiberState st)
    -> std::string
{
    switch (st)
    {
#define IFCODE(value, str) \
    case (value): \
        return #str\

    IFCODE(FiberState::INIT, INIT);
    IFCODE(FiberState::RUNNING, RUNNING);
    IFCODE(FiberState::READY, READY);
    IFCODE(FiberState::HOLD, HOLDj);
    IFCODE(FiberState::EXCEPT, EXCEPT);
    IFCODE(FiberState::TERM, TERM);

#undef IFCODE
    }
}
} //namespace FiberT