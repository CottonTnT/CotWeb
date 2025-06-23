
#include "logger/logevent.h"
#include "logger/logformatpattern.h"

#include <cassert>
#include <unordered_map>
#include <functional>

namespace {

enum class ParseState {
    NORMAL,
    PATTERN,
    SUBPATTERN,
};

} // namespace

namespace LogT {

class PatternItemProxyBase;


using ItemProduceFunc = std::function<Sptr<PatternItemProxyBase>(std::string)>;
auto RegisterFormatItem()
    -> std::unordered_map<std::string, ItemProduceFunc>;

using StatusItemProduceFunc = std::function<Sptr<PatternItemProxyBase>(std::string, std::string)>;
auto RegisterFormatStatusItem()
    -> std::unordered_map<std::string, StatusItemProduceFunc>;

void FormatPattern::StartParse_()
{
    static auto produce_func_map1 = []() -> std::unordered_map<std::string, ItemProduceFunc>{
        return RegisterFormatItem();
    }();

    static auto produce_func_map2 = []() -> std::unordered_map<std::string, StatusItemProduceFunc> {
        return RegisterFormatStatusItem();
    }();

    assert(produce_func_map1.size() != 0);
    auto normal_str = std::string {}; // to store the normal str
    auto state = ParseState::NORMAL;

    for (auto i = size_t {0}; i < pattern_.size(); i++)
    {
        switch (state)
        {
            case ParseState::NORMAL: {
                while (i < pattern_.size() and pattern_[i] != '%')
                {
                    normal_str.push_back(pattern_[i]);
                    i++;
                }
                if(not normal_str.empty()){
                    auto produce_func = produce_func_map2["str"];
                    pattern_items_.push_back(produce_func("str", normal_str));

                    // pattern_items_.back()->SetItem(std::string("str"));
                    // pattern_items_.back()->SetSubpattern(normal_str);

                }
                normal_str.clear();

                if(i >= pattern_.size()) break;
                // c == '%'
                assert(pattern_[i] == '%');
                state = ParseState::PATTERN;
                break;
            }
            case ParseState::PATTERN: {
                auto c = pattern_[i];
                // if (c == '%')
                // {
                //     auto produce_func = item_map[std::string(1, c)];
                //     pattern_items_.push_back(produce_func(std::string(1, c)));
                //     break;
                // }
                if (i + 1 < pattern_.size() and pattern_[i + 1] == '{')//是否有subpattern
                {
                    i ++;
                    state = ParseState::SUBPATTERN;
                    break;
                }
                if (produce_func_map1.find(std::to_string(c)) == produce_func_map1.end())
                {
                    //todo: exception
                }

                auto produce_func = produce_func_map1[std::string(1, c)];
                pattern_items_.push_back(produce_func(std::string(1, c)));
                state = ParseState::NORMAL;
                break;
            }
            case ParseState::SUBPATTERN: {
                auto escape_c = pattern_[i-2];
                auto sub_pattern = std::string {};
                while (i < pattern_.size() and pattern_[i] != '}')
                {
                    sub_pattern.push_back(pattern_[i]);
                    i++;
                }
                if (i >= pattern_.size())
                {
                    // todo:throw exception
                    error_ = true;
                    return;
                }
                auto produce_func = produce_func_map2[std::string(1, escape_c)];
                pattern_items_.push_back(produce_func("str", sub_pattern));
                state = ParseState::NORMAL;
            }
        }
    }
    //todo:throw exception
    assert(error_ == false);
    assert(state == ParseState::NORMAL);

    if (not normal_str.empty())
    {
        auto produce_func = produce_func_map2["str"];
        pattern_items_.push_back(produce_func("str", std::move(normal_str)));
        state = ParseState::NORMAL;
    }
}

auto FormatPattern::Format(std::ostream& os, Sptr<Event> event)
    -> void
{
    for (auto i : pattern_items_)
    {
         i->Format(os, event);
    }
}

auto FormatPattern::Format(Sptr<Event> event)
    -> std::string
{
    auto ss = std::stringstream{};
    for (auto& i : pattern_items_)
    {
        i->Format(ss, event);
    }
    return ss.str();
}

auto FormatPattern::Show()
    -> void
{
    for (auto& i : pattern_items_)
    {
        i->Show();
    }
}

} // namespace LogT