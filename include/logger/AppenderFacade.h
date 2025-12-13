#pragma once

// class FormatPattern;
class LogEvent;

class AppenderFacade {
protected:
    AppenderFacade() = default;
    auto operator=(const AppenderFacade&) -> AppenderFacade& = default;
    auto operator=(AppenderFacade&&) -> AppenderFacade&      = default;
public:
    AppenderFacade(const AppenderFacade&)            = default;
    AppenderFacade(AppenderFacade&&)                 = default;
    virtual void log(const LogEvent& event)  = 0;
    virtual ~AppenderFacade() = default;

};
