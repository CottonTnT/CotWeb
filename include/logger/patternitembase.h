#pragma once
#include "logevent.h"
#include <iostream>
#include <print>

namespace LogT {

class PatternItemProxyBase {

protected:
    PatternItemProxyBase& operator=(const PatternItemProxyBase&) = default;
    PatternItemProxyBase& operator=(PatternItemProxyBase&&)      = default;

public:
    PatternItemProxyBase() = default;
    PatternItemProxyBase(const PatternItemProxyBase&)            = default;
    PatternItemProxyBase(PatternItemProxyBase&&)                 = default;

    virtual auto Format(std::ostream& os, Sptr<Event> event)
        -> void = 0;

    virtual ~PatternItemProxyBase() = default;
};

} // namespace LogT