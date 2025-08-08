#pragma once

#include <stdexcept>
namespace FiberT{

class LibInnerError : public std::logic_error {

public:
    LibInnerError(std::string error_msg)
        : std::logic_error {"There is a vulnerability in this library:" + std::move(error_msg)}
    {
    }
};

class InitError : public std::runtime_error {
public:
    InitError(std::string error_msg)
        : std::runtime_error {"Init the fiber failed:" + std::move(error_msg)}
    {
    }
};


class SwapError : public std::runtime_error {
public:
    SwapError(std::string error_msg)
        : std::runtime_error {"swap the fiber failed:" + std::move(error_msg)}
    {
    }
};

class NullPointerError : public std::runtime_error {
public:
    NullPointerError(std::string error_msg)
        : std::runtime_error {"the fiber is null:" + std::move(error_msg)}
    {
    }
};





} //namespace FiberT