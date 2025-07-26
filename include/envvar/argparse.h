#include <string>
#include <vector>
#include <span>

namespace EnvT{

/**
 * @brief refine from https://github.com/p-ranav/argparse
 */
class ArgParser {
private:
    [[nodiscard]] auto ArgumentStartsWithPrefixChars_(const std::string& raw_seg) const
        -> bool;
public:
    [[nodiscard]] auto PreprocessArguments(const std::vector<std::string>& raw_arguments) const
        -> std::vector<std::string>;
    [[nodiscard]] auto PreprocessArguments(int argc, char** argv) const
        -> std::vector<std::string>;
    
private:
    std::string prefix_chars_{'-'};
    std::string assign_chars_{'='};
};

} //namespace EnvT