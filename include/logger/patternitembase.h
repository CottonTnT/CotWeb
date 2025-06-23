#pragma once
#include "logevent.h"
#include <iostream>
#include <print>

namespace LogT {

class PatternItemProxyBase {

protected:
    auto operator=(const PatternItemProxyBase&)
        -> PatternItemProxyBase& = default;
    auto operator=(PatternItemProxyBase&&)
        -> PatternItemProxyBase& = default;

public:
    PatternItemProxyBase()                            = default;
    PatternItemProxyBase(const PatternItemProxyBase&) = default;
    PatternItemProxyBase(PatternItemProxyBase&&)      = default;

    //     auto HaveSubpattern() const
    //         -> bool { return not sub_pattern_.empty(); }

    //     auto SetSubpattern(std::string sub_pattern)
    //         -> void {   sub_pattern_ = std::move(sub_pattern); }
    //     auto SetItem(std::string item)
    //         -> void {item_ = std::move(item);};
    virtual auto Show()
        -> void = 0;
    //     [[nodiscard]] auto GetSubpattern() const
    //         -> std::string_view{return sub_pattern_;}

    // virtual auto Format(std::ostream& os, Sptr<Event> event) -> void                     = 0;

    virtual auto Format(std::ostream& os, Sptr<Event> event)
        -> void = 0;

    virtual auto Clone(const PatternItemProxyBase& rhs)
        -> PatternItemProxyBase& = 0;

    virtual ~PatternItemProxyBase() = default;
};

} // namespace LogT