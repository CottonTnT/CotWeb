
#include <algorithm>

#include "config/configvarbase.h"
namespace ConfigT{

ConfigVarBase::ConfigVarBase(std::string name, std::string description_)
    : name_(std::move(name))
    , description_(std::move(description_))
{
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
}

auto ConfigVarBase::GetName() const&
    -> const std::string&
{
    return name_;
}

auto ConfigVarBase::GetName() &&
    -> std::string
{
    return std::move(name_);
}

auto ConfigVarBase::GetDescription() const&
    -> const std::string&
{
    return description_;
}

auto ConfigVarBase::GetDescription() &&
    -> std::string
{
    return std::move(description_);
}
} //namespace ConfigT