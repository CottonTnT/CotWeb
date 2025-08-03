#include "logger/logappender.h"
#include "common/util.h"
#include "logger/logger.h"

using namespace LogT;

auto main()
    -> int
{
    auto logger = std::make_shared<Logger>();
    logger->AddAppender(std::make_shared<Appender<StdoutAppender>>());
    {
        auto lg_gd = LogT::LogGuard{logger,      \
                      Sptr<LogT::Event>(new LogT::Event("nihao", Level::DEBUG, __LINE__, __FILE__, 0, UtilT::GetThreadId(),"tname_placeholder", 0, time(0)))         \
        };
        lg_gd.GetLogEvent()->GetSS() << "will repeat~";
    }
    return 0;
}