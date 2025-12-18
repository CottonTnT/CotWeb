#pragma once
#include "Logger.h"
#include "LoggerManager.h"

static auto log = GET_ROOT_LOGGER();

#define EASY_SYSFATAL(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_SYSERR(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_FATAL(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_ERROR(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_WARN(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_TRACE(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_INFO(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_DEBUG(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_ALL(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)


