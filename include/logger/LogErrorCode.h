#include <system_error>
#include <string>

enum class LogErrcCode:int {
    Success = 0,
    UnrecogizedLogPattern,
    NoAppender,
};

class LogErrorCategory : public std::error_category {
public:
    [[nodiscard]] auto name() const noexcept
        -> const char* override
    {
        return "LogError";
    }

    [[nodiscard]] auto message(int ev) const
        -> std::string override
    {
        switch (static_cast<LogErrcCode>(ev))
        {
            case LogErrcCode::Success:
                return "Success";
            case LogErrcCode::UnrecogizedLogPattern:
                return "Unrecogized log pattern";
            case LogErrcCode::NoAppender:
                return "No appender found";
            default:
                return "Unknown log error";
        }
    }
    [[nodiscard]] auto default_error_condition(int ev) const noexcept
        -> std::error_condition override
    {
        switch (static_cast<LogErrcCode>(ev))
        {
            case LogErrcCode::Success:
                return std::error_condition {static_cast<int>(LogErrcCode::Success), *this};
            case LogErrcCode::UnrecogizedLogPattern:
                return std::error_condition {static_cast<int>(LogErrcCode::UnrecogizedLogPattern), *this};

            case LogErrcCode::NoAppender:
                return std::error_condition {static_cast<int>(LogErrcCode::NoAppender), *this};
            default:
                break;
        }
        return std::error_condition {ev, *this};
    }
};

inline auto make_error_code(LogErrcCode e)
    -> std::error_code
{
    static auto s_Category = LogErrorCategory {};
    return {static_cast<int>(e), s_Category};
}


namespace std {
template <>
struct is_error_code_enum<LogErrcCode> : true_type
{
};
} // namespace std