#pragma once
#include <string_view>
#include <optional>

namespace LogT{

    enum class Level;
    auto LevelToString(Level level)
        ->  std::optional<std::string_view>;

    auto StringToLogLevel(std::string_view str)
        ->  std::optional<Level>;

} //namespace LogT