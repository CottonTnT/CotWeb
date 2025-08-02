#pragma once
#include "patternitembase.h"
#include <iostream>
#include <print>

namespace LogT {


template <typename T>
concept IsNormalPatternItem = requires(T x, std::ostream y, Sptr<Event> z)
{
    x.Format(y, z);
};

template <typename T>
concept IsStatusPatternItem = requires(T x, std::ostream y, Sptr<Event> z)
{
    x.Format(y, z);
    x.GetSubpattern();
};

template <typename T>
concept IsPatternItem = IsNormalPatternItem<T> or IsStatusPatternItem<T>;


template <typename ItemImpl>
requires (IsPatternItem<ItemImpl>)
class PatternItemProxy : public PatternItemProxyBase {
public:

    template <typename... Args>
    explicit PatternItemProxy(Args&&... args)
        : item_(std::forward<Args>(args)...)
    {
    }

    auto Format(std::ostream& os, Sptr<Event> event)
        -> void override
    {
        item_.Format(os, event);
    }

private:
    ItemImpl item_;
};

} // namespace LogT