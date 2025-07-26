#pragma once
#include <string_view>

namespace Env::Utils{

auto getEnvOr(std::string_view key, std::string default_value)
    -> std::string;

auto setEnv(std::string_view key, std::string_view val)
    -> bool;

auto getAbsolutePath(std::string_view path)
    -> std::string;

auto getAbsoluteWorkPath(std::string_view path)
    -> std::string;

} // namespace Env::Utils

