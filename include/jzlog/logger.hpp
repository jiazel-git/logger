#pragma once

#include "jzlog/core/log_level.h"
#include "jzlog/slinks/sink.h"
#include "logger_impl.hpp"
#include <memory>
#include <string_view>
#include <utility>
namespace jzlog
{
class CLogger {
public:
    CLogger( CLogger&& _oth )            = delete;
    CLogger& operator=( CLogger&& _oth ) = delete;
    ~CLogger()                           = default;

public:
    explicit CLogger( loglevel::LogLevel _lvl         = loglevel::LogLevel::TRACE,
                      std::string_view   _logger_name = "logger" ) :
        _impl( std::make_unique< CLoggerImpl >( _lvl, _logger_name ) ) {}

    template < class... _Args >
    void info( std::string_view _fmt, _Args&&... _args ) const noexcept {
        this->_impl->info( _fmt, std::forward< _Args >( _args )... );
    }

    template < class... _Args >
    void trace( std::string_view _fmt, _Args&&... _args ) const noexcept {
        this->_impl->trace( _fmt, std::forward< _Args >( _args )... );
    }

    template < class... _Args >
    void debug( std::string_view _fmt, _Args&&... _args ) const noexcept {
        this->_impl->debug( _fmt, std::forward< _Args >( _args )... );
    }

    template < class... _Args >
    void warn( std::string_view _fmt, _Args&&... _args ) const noexcept {
        this->_impl->warn( _fmt, std::forward< _Args >( _args )... );
    }

    template < class... _Args >
    void error( std::string_view _fmt, _Args&&... _args ) const noexcept {
        this->_impl->error( _fmt, std::forward< _Args >( _args )... );
    }

    template < class... _Args >
    void fatal( std::string_view _fmt, _Args&&... _args ) const noexcept {
        this->_impl->fatal( _fmt, std::forward< _Args >( _args )... );
    }

    void add_sink( std::unique_ptr< sinks::ISink > _sink ) const noexcept {
        _impl->add_sink( std::move( _sink ) );
    }

private:
    std::unique_ptr< CLoggerImpl > _impl;
};

}  // namespace jzlog
