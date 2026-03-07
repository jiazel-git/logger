#pragma once

#include "jzlog/core/log_level.h"
#include "jzlog/sinks/sink.h"
#include "logger_impl.hpp"
#include <memory>
#include <string_view>
#include <utility>
namespace jzlog
{
class CLogger {
public:
    CLogger( CLogger&& oth )            = delete;
    CLogger& operator=( CLogger&& oth ) = delete;
    ~CLogger()                          = default;

public:
    explicit CLogger( loglevel::LogLevel lvl         = loglevel::LogLevel::TRACE,
                      std::string_view   logger_name = "logger" ) :
        _impl( std::make_unique< CLoggerImpl >( lvl, logger_name ) ) {}

    template < class... Args >
    void info( std::string_view fmt, Args&&... args ) const noexcept {
        this->_impl->info( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    void trace( std::string_view fmt, Args&&... args ) const noexcept {
        this->_impl->trace( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    void debug( std::string_view fmt, Args&&... args ) const noexcept {
        this->_impl->debug( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    void warn( std::string_view fmt, Args&&... args ) const noexcept {
        this->_impl->warn( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    void error( std::string_view fmt, Args&&... args ) const noexcept {
        this->_impl->error( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    void fatal( std::string_view fmt, Args&&... args ) const noexcept {
        this->_impl->fatal( fmt, std::forward< Args >( args )... );
    }

    void add_sink( std::unique_ptr< sinks::ISink > sink ) const noexcept {
        _impl->add_sink( std::move( sink ) );
    }

private:
    std::unique_ptr< CLoggerImpl > _impl;
};

}  // namespace jzlog
