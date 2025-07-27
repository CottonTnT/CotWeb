#pragma once
#include "common.h"
namespace LogT{

class LogFormatter;
class Event;

template <typename T>
concept IsAppenderImpl = requires(T x, Sptr<LogFormatter> y, Sptr<Event> z) {
    x.Log(y, z);
};

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

} //namespace LogT