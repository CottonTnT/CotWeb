#include "logger/logevent.h"
#include "logger/logger.h"
#include "logger/loglevel.h"
#include <memory>

namespace LogT {

/* ======================== Event ======================== */
Event::Event(std::string_view logger_name,
      Level loglevel,
      uint32_t line_number,
      std::string_view filename,
      uint32_t elapse,
      uint32_t thread_id,
      std::string_view thread_name,
      uint32_t co_id,
      std::time_t timestamp)
    : logger_name_{logger_name}
    , loglevel_(loglevel)
    , line_number_(line_number)
    , filename_(filename)
    , elapse_(elapse)
    , thread_id_(thread_id)
    , thread_name_(thread_name)
    , co_id_(co_id)
    , timestamp_(timestamp)
{
}

void Event::Printf(const char* fmt, ...)
{
    va_list al;
    va_start(al, fmt);
    VPrintf_(fmt, al);
    va_end(al);
}

void Event::VPrintf_(const char* fmt, va_list al)
{
    char* buf = nullptr;
    int len   = vasprintf(&buf, fmt, al);
    auto _ = std::unique_ptr<char>(buf);
    if (len != -1)
    {
        custom_msg_ << std::string_view(buf, len);
    }
}



LogGuard::LogGuard(Sptr<Logger> logger, Sptr<Event> event)
        : logger_(logger)
        , event_(event)
{}


LogGuard::~LogGuard()
{
    logger_->Log(event_);
}

} //namespace LogT