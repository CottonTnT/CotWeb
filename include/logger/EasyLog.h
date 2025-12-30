#pragma once
#include "Logger.h"
#include "LoggerManager.h"

static auto log = GET_ROOT_LOGGER();

#define EASY_SYSFATAL(fmt, ...) LOG_SYSFATAL_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_SYSERR(fmt, ...) LOG_SYSERR_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_FATAL(fmt, ...) LOG_FATAL_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_ERROR(fmt, ...) LOG_ERROR_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_WARN(fmt, ...) LOG_WARN_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_TRACE(fmt, ...) LOG_TRACE_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_INFO(fmt, ...) LOG_INFO_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_DEBUG(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_ALL(fmt, ...) LOG_ALL_FMT(log, fmt, ##__VA_ARGS__)


#define SYSFATAL() LOG_SYSFATAL(log)
#define SYSERR() LOG_ERROR(log)
#define FATAL() LOG_FATAL(log)
#define ERROR() LOG_ERROR(log)
#define WARN() LOG_WARN(log)
#define TRACE() LOG_TRACE(log)
#define INFO() LOG_INFO(log)
#define DEBUG() LOG_DEBUG(log)
#define ALL() LOG_ALL(log)

