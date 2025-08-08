#pragma once

#include <cstddef>
#include <malloc.h>

class StackAllocator {
public:
    static auto Alloc(std::size_t size)
        -> void*
    {
        return malloc(size);
    }
    static auto Dealloc(void* vp)
        -> void
    {
        return free(vp);
    }

};