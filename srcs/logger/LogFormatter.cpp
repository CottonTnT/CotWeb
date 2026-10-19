#include "LogFormatter.h"
#include "FormatItem.h"
#include "LogRecordView.h"

#include <cassert>
#include <cstddef>

namespace {

enum class ParseState {
    NORMAL,
    PATTERN,
    SUBPATTERN,
};

} // namespace

namespace X::detail {

auto LogFormatter::format(const LogRecordView& event) const
    -> FormattedRecordBuffer
{
    auto buf = FormattedRecordBuffer { };

    for (const auto& item : formatItemList_) {
        item->format(event, buf);
    }
    return buf;
}

auto createKeyFormatItem(char key, std::string conf = "")
    -> std::unique_ptr<const FormatItem>;

[[nodiscard]] auto createLiteralFormatItem(std::string literal)
    -> std::unique_ptr<const FormatItem>;

auto LogFormatter::parsePattern_()
    -> int
{
    auto normal_str = std::string { }; // to store the normal str
    auto state = ParseState::NORMAL;

    for (auto i = size_t { 0 }; i < pattern_.size(); i++) {

        switch (state) {
        case ParseState::NORMAL: {
            while (i < pattern_.size() and pattern_[i] != '%') {
                normal_str.push_back(pattern_[i]);
                i++;
            }
            if (not normal_str.empty()) {
                formatItemList_.push_back(createLiteralFormatItem(normal_str));
            }
            normal_str.clear();

            if (i >= pattern_.size())
                break;
            // c == '%'
            assert(pattern_[i] == '%');
            state = ParseState::PATTERN;
            break;
        }
        case ParseState::PATTERN: {
            auto c = pattern_[i];
            if (i + 1 < pattern_.size() and pattern_[i + 1] == '{') // 是否有subpattern
            {
                i++;
                state = ParseState::SUBPATTERN;
                break;
            }

            // todo: assert
            // itemFactoryMap.find(std::to_string(c)) == itemFactoryMap.end()

            formatItemList_.push_back(createKeyFormatItem(c));
            state = ParseState::NORMAL;
            break;
        }
        case ParseState::SUBPATTERN: {
            auto escape_c = pattern_[i - 2];
            auto sub_pattern = std::string { };
            while (i < pattern_.size() and pattern_[i] != '}') {
                sub_pattern.push_back(pattern_[i]);
                i++;
            }
            if (i >= pattern_.size()) {
                return -1;
            }
            formatItemList_.push_back(createKeyFormatItem(escape_c, sub_pattern));
            state = ParseState::NORMAL;
        }
        }
    }
    assert(state == ParseState::NORMAL);
    if (not normal_str.empty()) {
        formatItemList_.push_back(createLiteralFormatItem(std::move(normal_str)));
        state = ParseState::NORMAL;
    }
    return 0;
}

} // namespace X::detail
