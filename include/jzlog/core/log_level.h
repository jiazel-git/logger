/**
 * @file log_level.h
 * @brief 日志级别定义和相关工具函数
 */
#pragma once

#include <algorithm>
#include <string>
#include <string_view>
namespace jzlog
{
namespace loglevel
{

/**
 * @enum LogLevel
 * @brief 日志级别枚举
 */
enum class LogLevel : int
{
    TRACE = 0,  // 追踪级别
    DEBUG,      // 调试级别
    INFO,       // 信息级别
    WARN,       // 警告级别
    ERROR,      // 错误级别
    FATAL,      // 致命错误级别
    OFF,        // 关闭日志
    ALL         // 所有日志
};

/**
 * @brief 将日志级别转换为字符串
 * @param level 日志级别
 * @return 日志级别的字符串表示
 */
[[nodiscard]] inline constexpr std::string_view to_string( LogLevel level ) noexcept {
    switch ( level ) {
    case ( LogLevel::TRACE ):
        return "TRACE";
    case ( LogLevel::DEBUG ):
        return "DEBUG";
    case ( LogLevel::INFO ):
        return "INFO";
    case ( LogLevel::WARN ):
        return "WARN";
    case ( LogLevel::ERROR ):
        return "ERROR";
    case ( LogLevel::FATAL ):
        return "FATAL";
    case ( LogLevel::OFF ):
        return "OFF";
    default:
        return "ALL";
    }
}

/**
 * @brief 从字符串转换为日志级别
 * @param str 日志级别字符串
 * @return 日志级别枚举值
 */
[[nodiscard]] inline LogLevel from_string( std::string_view str ) noexcept {
    auto upper_str = []( std::string_view s ) -> std::string {
        std::string ret{ s };
        std::transform( ret.begin(), ret.end(), ret.begin(), []( auto& ch ) {
            return std::toupper( ch );
        } );
        return ret;
    };
    std::string upper{ upper_str( str ) };
    LogLevel    level{ LogLevel::ALL };
    if ( upper == "TRACE" ) {
        level = LogLevel::TRACE;
    } else if ( upper == "DEBUG" ) {
        level = LogLevel::DEBUG;
    } else if ( upper == "INFO" ) {
        level = LogLevel::INFO;
    } else if ( upper == "WARN" ) {
        level = LogLevel::WARN;
    } else if ( upper == "FATAL" ) {
        level = LogLevel::FATAL;
    } else if ( upper == "OFF" ) {
        level = LogLevel::OFF;
    } else {
        level = LogLevel::ALL;
    }
    return level;
}

/**
 * @brief 判断两个日志级别是否相等
 * @param left 左操作数
 * @param right 右操作数
 * @return 相等返回 true，否则返回 false
 */
inline constexpr bool operator==( LogLevel left, LogLevel right ) noexcept {
    return static_cast< int >( left ) == static_cast< int >( right );
}

/**
 * @brief 判断两个日志级别是否不相等
 * @param left 左操作数
 * @param right 右操作数
 * @return 不相等返回 true，否则返回 false
 */
inline constexpr bool operator!=( LogLevel left, LogLevel right ) noexcept {
    return !( left == right );
}

/**
 * @brief 判断左操作数是否大于右操作数
 * @param left 左操作数
 * @param right 右操作数
 * @return 大于返回 true，否则返回 false
 */
inline constexpr bool operator>( LogLevel left, LogLevel right ) noexcept {
    return static_cast< int >( left ) > static_cast< int >( right );
}

/**
 * @brief 判断左操作数是否大于等于右操作数
 * @param left 左操作数
 * @param right 右操作数
 * @return 大于等于返回 true，否则返回 false
 */
inline constexpr bool operator>=( LogLevel left, LogLevel right ) noexcept {
    return static_cast< int >( left ) >= static_cast< int >( right );
}

/**
 * @brief 判断左操作数是否小于右操作数
 * @param left 左操作数
 * @param right 右操作数
 * @return 小于返回 true，否则返回 false
 */
inline constexpr bool operator<( LogLevel left, LogLevel right ) noexcept {
    return static_cast< int >( left ) < static_cast< int >( right );
}

/**
 * @brief 判断左操作数是否小于等于右操作数
 * @param left 左操作数
 * @param right 右操作数
 * @return 小于等于返回 true，否则返回 false
 */
inline constexpr bool operator<=( LogLevel left, LogLevel right ) noexcept {
    return static_cast< int >( left ) <= static_cast< int >( right );
}

/**
 * @brief 前置自增运算符
 * @param level 日志级别
 * @return 自增后的日志级别
 */
inline constexpr LogLevel operator++( LogLevel level ) noexcept {
    if ( level < LogLevel::OFF ) {
        level = static_cast< LogLevel >( static_cast< int >( level ) + 1 );
    }
    return level;
}

/**
 * @brief 后置自增运算符
 * @param level 日志级别
 * @param int 占位参数，表示后置运算
 * @return 自增前的日志级别
 */
inline constexpr LogLevel operator++( LogLevel level, int ) noexcept {
    LogLevel temp{ level };
    ++level;
    return temp;
}

/**
 * @brief 前置自减运算符
 * @param level 日志级别
 * @return 自减后的日志级别
 */
inline constexpr LogLevel operator--( LogLevel level ) noexcept {
    if ( level > LogLevel::TRACE ) {
        level = static_cast< LogLevel >( static_cast< int >( level ) - 1 );
    }
    return level;
}

/**
 * @brief 后置自减运算符
 * @param level 日志级别
 * @param int 占位参数，表示后置运算
 * @return 自减前的日志级别
 */
inline constexpr LogLevel operator--( LogLevel level, int ) noexcept {
    LogLevel temp{ level };
    --level;
    return temp;
}
}  // namespace loglevel
}  // namespace jzlog
