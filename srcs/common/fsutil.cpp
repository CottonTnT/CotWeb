#include "common/fsutil.h"
#include "logger/logger.h"
#include "logger/loggermanager.h"

#include "common/util.hpp"
#include <cstring>
#include <dirent.h>
#include <memory>
#include <unistd.h>

//todo:replace these api with C++17 filesystem api
namespace Utils::FSUtils{

static auto s_SysLogger = GET_LOGGER_BY_NAME("system");


auto ListAllFile(std::vector<std::string>& files, const std::string& path, const std::string& suffix)
    -> void
{
    auto sucess_or = UtilT::SyscallWrapper<-1>( access, path.c_str(),0);

    if (not sucess_or)
    {
        LOG_INFO(s_SysLogger) << sucess_or.error();
        return;
    }
    auto dir_or = UtilT::SyscallWrapper<nullptr>(opendir, path.c_str());

    if (not dir_or)
    {
        LOG_INFO(s_SysLogger) << dir_or.error();
        return;
    }


    auto dir = std::unique_ptr<DIR, std::function<void(DIR*)>>
    {
        dir_or.value(),
        [](DIR* dir)
                -> void {
                closedir(dir);
        }
    };

    struct dirent* dir_ent = nullptr;

    while ((dir_ent = readdir(dir.get())) != nullptr)
    {
        if (dir_ent->d_type == DT_DIR)
        {
            if (!strcmp(dir_ent->d_name, ".")
                or !strcmp(dir_ent->d_name, ".."))
                continue;
            ListAllFile(files,
                        path + "/" + dir_ent->d_name,
                        suffix);
            continue;
        }
        if (dir_ent->d_type == DT_REG)
        {
            auto fname = std::string{dir_ent->d_name};
            if(suffix.empty())
                files.push_back(path + "/" + fname);
            else
            {
                if (fname.size() < suffix.size())
                    continue;
                if (fname.ends_with(suffix))
                    files.push_back(path + "/" + fname);
            }
        }
        
    }
}

    
} //namespace Utils::FSUtils

