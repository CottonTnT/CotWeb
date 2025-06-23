
#include "common.h"
#include "mutex.h"
#include "loggerconcept.hpp"
#include "logformatpattern.h"


#include <memory>
#include <utility>

namespace LogT{

// class FormatPattern;
class Event;

class AppenderProxyBase {

public:
    using MutexType = Cot::SpinMutex;

    explicit AppenderProxyBase(Sptr<FormatPattern> default_formatter = std::make_shared<FormatPattern>());

    AppenderProxyBase(const AppenderProxyBase&)            = delete;
    AppenderProxyBase(AppenderProxyBase&&)                 = delete;
    auto operator=(const AppenderProxyBase&) -> AppenderProxyBase& = delete;
    auto operator=(AppenderProxyBase&&) -> AppenderProxyBase&      = delete;

    virtual void Log( Sptr<Event> event)  = 0;

    auto GetFormatPattern() const
        -> Sptr<FormatPattern>;

    auto SetFormatPattern(Sptr<FormatPattern> formatter)
        -> void;

    virtual ~AppenderProxyBase() = default;
protected:
    Sptr<FormatPattern> formatter_;
    mutable MutexType mtx_;
};

} //namespace LogT