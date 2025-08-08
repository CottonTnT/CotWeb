#pragma once

#include <unistd.h>
#include <string>
namespace CurThread{

    auto GetCurThrId()
        -> pid_t;
    
    auto GetCurThrName()
        -> std::string;

    auto SetCurThrName(std::string name)
        -> void;

    // auto GetCurThrHandle()
    //     -> Sptr<Thr::Thread>;

    // auto SetCurThrName(std::string name)
    //     -> void;
    
} //namespace CurThread