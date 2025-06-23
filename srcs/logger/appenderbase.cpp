
#include "logger/appenderbase.h"

namespace LogT{

AppenderProxyBase::AppenderProxyBase(Sptr<FormatPattern> default_formatter)
    : formatter_(default_formatter)
{
}

void AppenderProxyBase::SetFormatPattern(Sptr<FormatPattern> formatter)
{
    auto lk_guard = std::lock_guard<MutexType>(mtx_);
    formatter_   = formatter;
}

auto AppenderProxyBase::GetFormatPattern() const
    -> Sptr<FormatPattern>
{
    auto lk_guard = std::lock_guard<MutexType>(mtx_);
    return formatter_;
}

} //namespace LogT