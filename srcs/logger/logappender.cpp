#include "logger/logappender.h"
namespace LogT{


/* ======================== StdoutAppender ======================== */
void StdoutAppender::Log(Sptr<LogFormatter> fmter, Sptr<Event> event)
{
    fmter->Format(std::cout,  event);
}


/* ======================== FileAppender ======================== */

//todo:exception
FileAppender::FileAppender(std::string_view filename)
    : filename_(filename)
{
    Reopen();
}

void FileAppender::Log(Sptr<LogFormatter> fmter, Sptr<Event> event)
{
    filestream_ << fmter->Format(event);
}

auto FileAppender::Reopen()
    -> bool
{
    if (filestream_)
    {
        filestream_.close();
    }
    filestream_.open(filename_);

    return  !! filestream_;
}

} //namespace LogT