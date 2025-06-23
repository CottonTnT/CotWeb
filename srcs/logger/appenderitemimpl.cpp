#include "logger/logappender.hpp"
namespace LogT{


/* ======================== StdoutAppender ======================== */
void StdoutAppender::Log(Sptr<FormatPattern> fmter, Sptr<Event> event)
{
    fmter->Format(std::cout,  event);
    std::cout << event->GetContent();
}


/* ======================== FileAppender ======================== */

//todo:exception
FileAppender::FileAppender(std::string filename)
    : filename_(filename)
{
    Reopen();
}

void FileAppender::Log(Sptr<FormatPattern> fmter, Sptr<Event> event)
{
    filestream_ << fmter->Format(event);
    filestream_ << event->GetContent();
}

auto FileAppender::Reopen()
    -> bool
{
    if (filestream_)
    {
        filestream_.close();
    }
    filestream_.open(filename_);

    return not not filestream_;
}

} //namespace LogT