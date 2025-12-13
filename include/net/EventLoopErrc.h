#include <string>
#include <system_error>

enum class EventLoopErrcCode {
    NotInOwnerLoop = 1,
    AlreadyInLooping,
    AlreadyQuit,
    NotLoopingYet,
};

class EventLoopErrcCategory : public std::error_category {
    [[nodiscard]] auto name() const noexcept
        -> const char* override
    {
        return "EventLoopError";
    }

    [[nodiscard]] auto message(int ev) const
        -> std::string override
    {
        switch (static_cast<EventLoopErrcCode>(ev))
        {
            case EventLoopErrcCode::NotInOwnerLoop:
                return "Operation not in owner EventLoop thread";
            case EventLoopErrcCode::AlreadyInLooping:
                return "EventLoop is already looping";
            case EventLoopErrcCode::AlreadyQuit:
                return "Quit called when EventLoop quit already";
            case EventLoopErrcCode::NotLoopingYet:
                return "Quit called when EventLoop is not looping yet";
            default:
                return "Unknown EventLoop error";
        }
    }

    [[nodiscard]] auto default_error_condition(int ev) const noexcept
        -> std::error_condition override
    {
        switch (static_cast<EventLoopErrcCode>(ev))
        {
            case EventLoopErrcCode::NotInOwnerLoop:
                return std::error_condition {static_cast<int>(EventLoopErrcCode::NotInOwnerLoop), *this};
            case EventLoopErrcCode::AlreadyInLooping:
                return std::error_condition {static_cast<int>(EventLoopErrcCode::AlreadyInLooping), *this};
            case EventLoopErrcCode::AlreadyQuit:
                return std::error_condition {static_cast<int>(EventLoopErrcCode::AlreadyQuit), *this};
            case EventLoopErrcCode::NotLoopingYet:
                return std::error_condition {static_cast<int>(EventLoopErrcCode::NotLoopingYet), *this};
            default:
                break;
        }
        return std::error_condition {ev, *this};
    }
};

inline auto make_error_code(EventLoopErrcCode e)
    -> std::error_code
{
    static auto s_category = EventLoopErrcCategory {};
    return {static_cast<int>(e), s_category};
}

namespace std {
template <>
struct is_error_code_enum<EventLoopErrcCode> : true_type
{
};
} // namespace std
