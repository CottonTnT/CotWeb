#pragma once
#include "common.h"
namespace LogT{

class FormatPattern;
class Event;

template <typename T>
concept IsAppenderImpl = requires(T x, Sptr<FormatPattern> y, Sptr<Event> z) {
    x.Log(y, z);
};

template <typename T>
concept IsNormalPatternItem = requires(T x, std::ostream y, Sptr<Event> z)
{
    x.Format(y, z);
};

template <typename T>
concept IsStatusPatternItem = requires(T x, std::ostream y, Sptr<Event> z, std::string str)
{
    x.Format(y, z, str);
};

template <typename T>
concept IsPatternItem = IsNormalPatternItem<T> or IsStatusPatternItem<T>;

} //namespace LogT