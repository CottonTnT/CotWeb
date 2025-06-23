#pragma once
#include <memory>

template <typename T>
using Sptr = std::shared_ptr<T>;

template <typename T, typename Deleter = std::default_delete<T>>
using Uptr = std::unique_ptr<T, Deleter>;

