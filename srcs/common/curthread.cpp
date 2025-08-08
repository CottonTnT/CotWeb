#pragma once

#include "common/util.h"
#include "common/thread.h"
#include <unistd.h>
#include <string>
namespace CurThread{

    static constexpr pid_t c_NotSet = -1;

    static thread_local pid_t t_CachedTid = c_NotSet ;
    static thread_local std::string  t_ThreadName = "UnKnown";

    static thread_local Thr::Thread* t_ThreadHandle = nullptr;
    auto GetCurThrId()
        -> pid_t
    {
        if (t_CachedTid == c_NotSet)
        {
            t_CachedTid = UtilT::GetThreadId();
        }
        return t_CachedTid;
    }

    auto GetCurThrName()
        -> std::string
    {
        return t_ThreadName;
    }

    auto SetCurThrName(std::string name)
        -> void; 
    
} //namespace CurThread