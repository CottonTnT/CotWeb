#pragma once

#include "appenderproxy.hpp"
#include "logformatter.h"
#include  <fstream>

namespace LogT{

class StdoutAppender {
public:
    static void Log(Sptr<LogFormatter> fmter, Sptr<Event> event);
};


//todo:to test, to make it better
class FileAppender {
public:

    explicit FileAppender(std::string_view filename);

    void Log(Sptr<LogFormatter> fmter, Sptr<Event> event);

    auto Reopen()
        -> bool;

private:
    //文件路径
    std::string filename_;
    std::ofstream filestream_;
    //上次打开的时间
    uint64_t lasttime_ = 0;
    // 文件打开错误标识
    bool reopen_error_ = false;
};

// todo
class SocketAppender;


//todo
class SystemAppender;


} //namespace LogT