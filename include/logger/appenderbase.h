
#include "common/alias.h"
#include "logformatter.h"
namespace LogT{

// class FormatPattern;
class Event;

class AppenderProxyBase {

public:

    AppenderProxyBase() = default;
    AppenderProxyBase(const AppenderProxyBase&)            = delete;
    AppenderProxyBase(AppenderProxyBase&&)                 = delete;
    auto operator=(const AppenderProxyBase&) -> AppenderProxyBase& = delete;
    auto operator=(AppenderProxyBase&&) -> AppenderProxyBase&      = delete;

    virtual void Log( Sptr<Event> event)  = 0;
    virtual ~AppenderProxyBase() = default;

};

} //namespace LogT