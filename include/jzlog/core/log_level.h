#pragma once

#include <algorithm>
#include <string>
#include <string_view>
namespace jzlog
{
namespace loglevel
{
enum class LogLevel : int
{
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    OFF,
    ALL
};

inline constexpr std::string_view to_string( LogLevel _level ) noexcept {
    switch ( _level ) {
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
inline LogLevel from_string( std::string_view _str ) noexcept {
    auto upper_str = []( std::string_view _str ) -> std::string {
        std::string ret{ _str };
        std::transform( ret.begin(), ret.end(), ret.begin(), []( auto& _Ch ) {
            return std::toupper( _Ch );
        } );
        return ret;
    };
    std::string upper{ upper_str( _str ) };
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
inline constexpr bool operator==( LogLevel _left, LogLevel _right ) noexcept {
    return static_cast< int >( _left ) == static_cast< int >( _right );
}
inline constexpr bool operator!=( LogLevel _left, LogLevel _right ) noexcept {
    return !( _left == _right );
}
inline constexpr bool operator>( LogLevel _left, LogLevel _right ) noexcept {
    return static_cast< int >( _left ) > static_cast< int >( _right );
}
inline constexpr bool operator>=( LogLevel _left, LogLevel _right ) noexcept {
    return static_cast< int >( _left ) >= static_cast< int >( _right );
}
inline constexpr bool operator<( LogLevel _left, LogLevel _right ) noexcept {
    return static_cast< int >( _left ) < static_cast< int >( _right );
}
inline constexpr bool operator<=( LogLevel _left, LogLevel _right ) noexcept {
    return static_cast< int >( _left ) <= static_cast< int >( _right );
}
inline constexpr LogLevel operator++( LogLevel _level ) noexcept {
    if ( _level < LogLevel::OFF ) {
        _level = static_cast< LogLevel >( static_cast< int >( _level ) + 1 );
    }
    return _level;
}
inline constexpr LogLevel operator++( LogLevel _level, int ) noexcept {
    LogLevel temp{ _level };
    ++_level;
    return temp;
}
inline constexpr LogLevel operator--( LogLevel _level ) noexcept {
    if ( _level > LogLevel::TRACE ) {
        _level = static_cast< LogLevel >( static_cast< int >( _level ) - 1 );
    }
    return _level;
}
inline constexpr LogLevel operator--( LogLevel _level, int ) noexcept {
    LogLevel temp{ _level };
    --_level;
    return temp;
}
}  // namespace loglevel
}  // namespace jzlog
