#pragma once
#include "LogFormatter.h"
#include "LogRecordView.h"

namespace X::detail {
class FormatItem {

protected:
    auto operator=(const FormatItem&) -> FormatItem& = default;
    auto operator=(FormatItem&&) -> FormatItem& = default;

public:
    FormatItem() = default;
    FormatItem(const FormatItem&) = default;
    FormatItem(FormatItem&&) = default;

    virtual auto format(const LogRecordView& event, FormattedRecordBuffer& buf) const
        -> void = 0;

    virtual ~FormatItem() = default;
};
} // namespace X::detail
