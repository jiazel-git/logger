#pragma once
#include "log_level.h"
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
namespace jzlog
{
using namespace loglevel;
struct SourceLocation {
    const char* _file_name;
    const char* _function_name;
    uint32_t    _line;
    uint32_t    _column;

    std::string to_string() const {
        std::stringstream ss;
        ss << "file_name:" << _file_name << " func_name:" << _function_name << " line:" << _line
           << " column:" << _column;
        return ss.str();
    }
};
struct LogRecord {
    std::chrono::system_clock::time_point _timestamp;
    LogLevel                              _level;
    std::string_view                      _logger_name;
    std::string_view                      _message;
    std::thread::id                       _thread_id;
    SourceLocation                        _loaction;

    LogRecord()                                   = default;
    LogRecord( const LogRecord& _oth )            = default;
    LogRecord& operator=( const LogRecord& _oth ) = default;
    LogRecord( LogRecord&& _oth )                 = default;
    LogRecord& operator=( LogRecord&& _oth )      = default;
    ~LogRecord()                                  = default;

    LogRecord( LogLevel _lvl, std::string_view _logger ) noexcept :
        _timestamp( std::chrono::system_clock::now() ),
        _level( _lvl ),
        _logger_name( _logger ),
        _message( "" ),
        _thread_id(),
        _loaction() {}

    bool is_valid() const noexcept { return !_message.empty() && _level != LogLevel::OFF; }

    std::string to_string() const {
        auto              t = std::chrono::system_clock::to_time_t( _timestamp );
        std::stringstream ss;
        ss << "timestamp:" << std::put_time( std::localtime( &t ), "%Y-%m-%d %H:%M:%s" )
           << " level:" << static_cast< uint8_t >( _level ) << " logger_name:" << _logger_name
           << " message:" << _message << " thread_id" << _thread_id
           << " location:" << _loaction.to_string();
        return ss.str();
    }
};
}  // namespace jzlog
