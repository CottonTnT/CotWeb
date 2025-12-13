
#include "logger/LogEvent.h"
#include "logger/LogFormatter.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <unordered_map>
#include <functional>
namespace {

enum class ParseState {
    NORMAL,
    PATTERN,
    SUBPATTERN,
};

} // namespace


class PatternItemFacade;


using ItemFactoryFunc = std::function<Sptr<PatternItemFacade>()>;

auto RegisterItemFactoryFunc()
    -> std::unordered_map<std::string, ItemFactoryFunc>;

using StatusItemFactoryFunc = std::function<Sptr<PatternItemFacade>(std::string)>;

auto RegisterStatusItemFactoryFunc()
    -> std::unordered_map<std::string, StatusItemFactoryFunc>;

void LogFormatter::startParse_()
{
    static auto s_ProduceFuncMap1 = []()
        -> std::unordered_map<std::string, ItemFactoryFunc>{
        return RegisterItemFactoryFunc();
    }();

    static auto s_ProduceFuncMap2 = []()
        -> std::unordered_map<std::string, StatusItemFactoryFunc> {
        return RegisterStatusItemFactoryFunc();
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

auto LogFormatter::format(std::ostream& os, const LogEvent& event) const
    -> size_t
{
    auto total_size = size_t {0};
    std::ranges::for_each(pattern_items_,
                          [&os, &event, &total_size ](const Sptr<PatternItemFacade> item)
                             -> void {
                            total_size += item->format(os, event);
                          });
    return total_size;
}

auto LogFormatter::format(const LogEvent& event) const
    -> std::string
{
    auto ss = std::stringstream{};
    std::ranges::for_each(pattern_items_,
                          [&ss, &event ](const  Sptr<PatternItemFacade> item)
                             -> void {
                            item->format(ss, event);
                          });
    return ss.str();
}
