
#include "logger/logevent.h"
#include "logger/logformatter.h"

#include <algorithm>
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


using ItemProduceFunc = std::function<Sptr<PatternItemProxyBase>()>;

auto RegisterFormatItem()
    -> std::unordered_map<std::string, ItemProduceFunc>;

using StatusItemProduceFunc = std::function<Sptr<PatternItemProxyBase>(std::string)>;

auto RegisterFormatStatusItem()
    -> std::unordered_map<std::string, StatusItemProduceFunc>;

void LogFormatter::StartParse_()
{
    static auto s_ProduceFuncMap1 = []()
        -> std::unordered_map<std::string, ItemProduceFunc>{
        return RegisterFormatItem();
    }();

    static auto s_ProduceFuncMap2 = []()
        -> std::unordered_map<std::string, StatusItemProduceFunc> {
        return RegisterFormatStatusItem();
    }();

    assert(s_ProduceFuncMap1.size() != 0);
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
                    auto produce_func = s_ProduceFuncMap2["str"];
                    pattern_items_.push_back(produce_func(normal_str));
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
                if (i + 1 < pattern_.size() and pattern_[i + 1] == '{')//是否有subpattern
                {
                    i ++;
                    state = ParseState::SUBPATTERN;
                    break;
                }
                if (s_ProduceFuncMap1.find(std::to_string(c)) == s_ProduceFuncMap1.end())
                {
                    //todo: exception
                }

                auto produce_func = s_ProduceFuncMap1[std::string(1, c)];
                pattern_items_.push_back(produce_func());
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
                auto produce_func = s_ProduceFuncMap2[std::string(1, escape_c)];
                pattern_items_.push_back(produce_func(sub_pattern));
                state = ParseState::NORMAL;
            }
        }
    }
    //todo:throw exception
    assert(error_ == false);
    assert(state == ParseState::NORMAL);

    if (not normal_str.empty())
    {
        auto produce_func = s_ProduceFuncMap2["str"];
        pattern_items_.push_back(produce_func(std::move(normal_str)));
        state = ParseState::NORMAL;
    }
}

auto LogFormatter::Format(std::ostream& os, Sptr<Event> event)
    -> void
{
    std::ranges::for_each(pattern_items_,
                          [&os, event ](const Sptr<PatternItemProxyBase> item)
                             -> void {
                            item->Format(os, event);
                          });
}

auto LogFormatter::Format(Sptr<Event> event)
    -> std::string
{
    auto ss = std::stringstream{};
    std::ranges::for_each(pattern_items_,
                          [&ss, event ](const  Sptr<PatternItemProxyBase> item)
                             -> void {
                            item->Format(ss, event);
                          });
    return ss.str();
}

} // namespace LogT