#pragma once
#include <string_view>

namespace LogT{

    enum class Level;
    auto LevelToString(Level level)
        ->  std::string_view;

    auto StringToLogLevel(std::string_view str)
        ->  Level;

} //namespace LogT