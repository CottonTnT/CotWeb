#pragma once
#include "patternitembase.h"
#include "loggerconcept.hpp"
#include <iostream>
#include <print>

namespace LogT {



template <typename ItemImpl>
requires (IsPatternItem<ItemImpl>)
class PatternItemProxy : public PatternItemProxyBase {
public:

    template <typename... Args>
    explicit PatternItemProxy(Args&&... args)
        : item_(std::forward<Args>(args)...)
    {
    }

    auto SetItem(std::string item)
        ->void { item_ = std::move(item); }

    auto Format(std::ostream& os, Sptr<Event> event)
        -> void override
    {
        item_.Format(os, event);
    }

    // auto Show()
    //     -> void override
    // {
    //     if constexpr (IsStatusPatternItem<ItemImpl>)
    //         std::print("{}:{}-", item_, item_.GetSubpattern());
    //     else 
    //         std::print("{}-", item_);
    // }

    // todo:finish clone
    auto Clone(const PatternItemProxyBase& rhs)
        -> PatternItemProxy& override
    {
        auto* under_type = dynamic_cast<const PatternItemProxy*>(&rhs);
        if(under_type != nullptr)
        {
            *this = static_cast<PatternItemProxy>(*under_type);
            return *this;
        }
        // todo:exception
        return *this;
    }

private:
    ItemImpl item_;
};

} // namespace LogT