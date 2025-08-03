#pragma once

#include <stdexcept>
namespace FiberT{

class FiberLibInnerError : public std::logic_error {

public:
    FiberLibInnerError(std::string error_msg)
        : std::logic_error {"There is a vulnerability in this library:" + std::move(error_msg)}
    {
    }
};

class FiberInitError : public std::runtime_error {
public:
    FiberInitError(std::string error_msg)
        : std::runtime_error {"Init the fiber failed:" + std::move(error_msg)}
    {
    }
};


class FiberSwapError : public std::runtime_error {
public:
    FiberSwapError(std::string error_msg)
        : std::runtime_error {"swap the fiber failed:" + std::move(error_msg)}
    {
    }
};



} //namespace FiberT