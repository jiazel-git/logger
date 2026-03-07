#include "jzlog/core/log_builder.h"
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/sinks/sink.h"
#include "jzlog/utils/thread_safe_queue.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace jzlog
{
class CLoggerImpl {
public:
    explicit CLoggerImpl( loglevel::LogLevel lvl, std::string_view logger_name ) noexcept :
        _running( false ), _log_builder( lvl, logger_name ) {
        start();
    }

    CLoggerImpl( CLoggerImpl&& oth )            = delete;

    CLoggerImpl& operator=( CLoggerImpl&& oth ) = delete;

    ~CLoggerImpl() { stop(); }

public:
    template < class... Args >
    inline void info( std::string_view fmt, Args&&... args ) noexcept {
        add_record( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    inline void trace( std::string_view fmt, Args&&... args ) noexcept {
        add_record( fmt, std::forward< Args >( args )... );
    }
    template < class... Args >
    inline void debug( std::string_view fmt, Args&&... args ) noexcept {
        add_record( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    inline void warn( std::string_view fmt, Args&&... args ) noexcept {
        add_record( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    inline void error( std::string_view fmt, Args&&... args ) noexcept {
        add_record( fmt, std::forward< Args >( args )... );
    }

    template < class... Args >
    inline void fatal( std::string_view fmt, Args&&... args ) noexcept {
        add_record( fmt, std::forward< Args >( args )... );
    }

    void add_sink( std::unique_ptr< sinks::ISink > sink ) noexcept {
        _sinks.emplace_back( std::move( sink ) );
    }

private:
    void log( LogRecord&& record ) const noexcept {
        for ( auto& slink : _sinks ) {
            slink->write( std::move( record ) );
        }
    }

    void start() noexcept {
        if ( !_running.exchange( true ) ) {
            _thread = std::thread( &CLoggerImpl::work_thread, this );
        }
    }

    void stop() noexcept {
        if ( _running.exchange( false ) ) {
            if ( _thread.joinable() ) {
                _thread.join();
            }
        }
    }

    void work_thread() {
        for ( ;; ) {
            if ( _running ) {
                LogRecord record;
                if ( _records.try_pop( record ) && record.is_valid() ) {
                    log( std::move( record ) );
                } else {
                    std::this_thread::sleep_for( std::chrono::microseconds{ 10 } );
                }
            } else {
                break;
            }
        }
        LogRecord record;
        while ( _records.try_pop( record ) ) {
            if ( record.is_valid() ) {
                log( std::move( record ) );
            }
        }
    }

    template < class... Args >
    void add_record( std::string_view fmt, Args&&... args ) noexcept {
        char             fmt_cstr[ 1024 ] = { 0 };
        constexpr size_t max_copy         = sizeof( fmt_cstr ) - 1;
        size_t           len              = std::min( max_copy, fmt.size() );
        std::copy_n( fmt.begin(), len, fmt_cstr );

        fmt_cstr[ len ]  = 0;

        char buf[ 1024 ] = { 0 };
        int  written = snprintf( buf, sizeof( buf ), fmt_cstr, std::forward< Args >( args )... );
        _log_builder.set_message( buf );
        LogRecord record = _log_builder.build();
        std::cout << "record:" << record.to_string();
        _records.push( record );
    }

private:
    std::atomic_bool                               _running;
    std::thread                                    _thread;
    CThreadSafeQueue< LogRecord >                  _records;
    CLogBuilder                                    _log_builder;
    std::vector< std::unique_ptr< sinks::ISink > > _sinks;
};

}  // namespace jzlog
