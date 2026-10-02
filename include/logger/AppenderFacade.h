#pragma once

// class FormatPattern;
class LogRecordView;

class AppenderFacade {
protected:
    AppenderFacade() = default;
    auto operator=(const AppenderFacade&) -> AppenderFacade& = default;
    auto operator=(AppenderFacade&&) -> AppenderFacade& = default;

public:
    AppenderFacade(const AppenderFacade&) = default;
    AppenderFacade(AppenderFacade&&) = default;
    virtual void log(const LogRecordView& event) = 0;
    virtual ~AppenderFacade() = default;
};
