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
    // PatternItemProxy()                                   = default;
    auto operator=(const PatternItemProxy&) -> PatternItemProxy& = default;
    auto operator=(PatternItemProxy&&) -> PatternItemProxy&      = default;

    PatternItemProxy(const PatternItemProxy&)            = default;
    PatternItemProxy(PatternItemProxy&&)                 = default;
    ~PatternItemProxy() override                         = default;

    explicit PatternItemProxy(std::string item)
        : item_(std::move(item)){}

    auto SetItem(std::string item)
        ->void { item_ = std::move(item); }

    auto Format(std::ostream& os, Sptr<Event> event)
        -> void override
    {
        item_impl_.Format(os, event);
    }

    auto Show()
        -> void override
    {
        std::print("{}-", item_);
    }

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
    ItemImpl item_impl_;
    std::string item_;
};

template <typename ItemImpl>
requires (IsStatusPatternItem<ItemImpl>)
class PatternItemProxy<ItemImpl>: public PatternItemProxyBase {
public:

    auto operator=(const PatternItemProxy&) -> PatternItemProxy& = default;
    auto operator=(PatternItemProxy&&) -> PatternItemProxy&      = default;

    PatternItemProxy(const PatternItemProxy&)            = default;
    PatternItemProxy(PatternItemProxy&&)                 = default;
    ~PatternItemProxy() override                         = default;


    PatternItemProxy(std::string item, std::string sub_pattern)
        : item_(std::move(item))
        , sub_pattern_(std::move(sub_pattern)) {}

    [[nodiscard]] auto GetSubpattern() const
        -> std::string_view{return sub_pattern_;}

    auto Format(std::ostream& os, Sptr<Event> event)
        -> void override
    {
        item_impl_.Format(os, event, std::string(GetSubpattern()));
    }

    auto Show()
        -> void override
    {
        std::print("{}:{}-", item_, sub_pattern_);
    }

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
    ItemImpl item_impl_;
    std::string item_;
    std::string sub_pattern_;
};

} // namespace LogT