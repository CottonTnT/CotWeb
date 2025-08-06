#include "fiber/fiberstate.h"
#include <string>
namespace FiberT{
auto FiberStateToString(FiberState st)
    -> std::string
{
    switch (st)
    {
#define IFCODE(value, str) \
    case (value): \
        return #str\

    IFCODE(FiberState::RUNNING, RUNNING);
    IFCODE(FiberState::READY, READY);
    IFCODE(FiberState::TERM, TERM);
#undef IFCODE
    }
    
}

} //namespace FiberT


