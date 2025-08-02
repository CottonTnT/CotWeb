#pragma once
#include <vector>
#include <string>

namespace Utils::FSUtils{

auto ListAllFile(std::vector<std::string>& files,
                 const std::string& path,
                 const std::string& subfix)
    -> void;

} //namespace Utils::FSUtils