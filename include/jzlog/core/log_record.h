/**
 * @file log_record.h
 * @brief 日志记录结构体定义
 */
#pragma once
#include "log_level.h"
#include <chrono>
#include <string>
#include <thread>
namespace jzlog
{
using namespace loglevel;

/**
 * @struct LogRecord
 * @brief 日记录结构体，存储日志的相关信息
 */
struct LogRecord {
    std::chrono::system_clock::time_point _timestamp;    // 时间戳
    LogLevel                              _level;        // 日志级别
    std::string                           _logger_name;  // 日志记录器名称
    std::string                           _message;      // 日志消息
    std::thread::id                       _thread_id;    // 线程ID
    std::string                           _function;     // 函数名
    int                                   _line;         // 行号
};
}  // namespace jzlog
