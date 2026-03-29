#pragma once
#include "log_level.h"
#include <chrono>
#include <string>
#include <thread>
namespace jzlog
{
using namespace loglevel;

struct LogRecord {
    std::chrono::system_clock::time_point _timestamp;
    LogLevel                              _level;
    std::string                           _logger_name;
    std::string                           _message;
    std::thread::id                       _thread_id;
    std::string                           _function;
    int                                   _line;
};
}  // namespace jzlog
