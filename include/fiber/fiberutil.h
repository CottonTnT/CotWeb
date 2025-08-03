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

    IFCODE(FiberState::INIT, INIT);
    IFCODE(FiberState::EXCEPT, EXCEPT);
    IFCODE(FiberState::HOLD, HOLD);
    IFCODE(FiberState::READY, READY);
    IFCODE(FiberState::TERM, TERM);
    IFCODE(FiberState::EXEC, EXEC);
#undef IFCODE
    }
    
}

} //namespace FiberT


