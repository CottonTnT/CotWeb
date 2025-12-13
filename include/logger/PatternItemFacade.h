#pragma once
#include "LogEvent.h"
#include <iostream>
#include <print>


class PatternItemFacade {

protected:
    auto operator=(const PatternItemFacade&) -> PatternItemFacade& = default;
    auto operator=(PatternItemFacade&&) -> PatternItemFacade&      = default;

public:
    PatternItemFacade() = default;
    PatternItemFacade(const PatternItemFacade&)            = default;
    PatternItemFacade(PatternItemFacade&&)                 = default;

    virtual auto format(std::ostream& os, const LogEvent& event)
        -> size_t = 0;

    virtual ~PatternItemFacade() = default;
};
