#pragma once
#include <string>


namespace ConfigT{

class ConfigVarBase {
public:
    ConfigVarBase(std::string name, std::string description_);

    ConfigVarBase(const ConfigVarBase&)            = default;
    ConfigVarBase(ConfigVarBase&&)                 = default;
    ConfigVarBase& operator=(const ConfigVarBase&) = default;
    ConfigVarBase& operator=(ConfigVarBase&&)      = default;

    auto GetName() const &
        -> const std::string&;

    auto GetName() &&
        -> std::string;

    [[nodiscard]] auto GetDescription() const&
        -> const std::string&;

    auto GetDescription() &&
        -> std::string;

    virtual auto ToString()
        -> std::string = 0;

    virtual auto FromString(const std::string& val)
        -> bool = 0;

    [[nodiscard]] virtual auto GetTypeName() const 
        -> std::string  = 0;


    virtual ~ConfigVarBase() = default;
protected:
    std::string  name_;
    std::string  description_;
};
} //namespace ConfigT