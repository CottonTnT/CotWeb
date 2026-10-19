#pragma once

#include "FormatItem.h"
#include "LogFormatter.h"
#include "LogRecordView.h"

namespace X::detail {

template <typename ItemImpl>
class FormatItemProxy : public FormatItem {
public:
    template <typename... Args>
    explicit FormatItemProxy(Args&&... args)
        : item_ { std::forward<Args>(args)... }
    {
    }

    auto format(const LogRecordView& event, FormattedRecordBuffer& buf) const
        -> void override
    {
        item_.format(event, buf);
    }

private:
    ItemImpl item_;
};
} // namespace X::detail
