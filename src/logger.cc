#include "jzlog/logger.h"
#include "jzlog/core/log_builder.h"
#include "jzlog/core/log_record.h"
#include "jzlog/slinks/sink.h"
#include "jzlog/utils/thread_safe_queue.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace jzlog
{
class CLogger::CLoggerImpl {
public:
    explicit CLoggerImpl() noexcept : _running( false ) { start(); }

    CLoggerImpl( CLoggerImpl&& _oth )            = delete;

    CLoggerImpl& operator=( CLoggerImpl&& _oth ) = delete;

    ~CLoggerImpl() { stop(); }

public:
    template < class... _Args >
    inline void info( std::string_view _fmt, _Args&&... _args ) noexcept {}

    template < class... _Args >
    inline void trace( std::string_view _fmt, _Args&&... _args ) noexcept {}
    template < class... _Args >
    inline void debug( std::string_view _fmt, _Args&&... _args ) noexcept {}

    template < class... _Args >
    inline void warn( std::string_view _fmt, _Args&&... _args ) noexcept {}

    template < class... _Args >
    inline void error( std::string_view _fmt, _Args&&... _args ) noexcept {}

    template < class... _Args >
    inline void fatal( std::string_view _fmt, _Args&&... _args ) noexcept {}

private:
    void log( LogRecord&& _record ) const noexcept {
        for ( auto& slink : _sinks ) {
            slink->write( std::move( _record ) );
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

private:
    std::atomic_bool                               _running;
    std::thread                                    _thread;
    CThreadSafeQueue< LogRecord >                  _records;
    CLogBuilder                                    _log_builder;
    std::vector< std::unique_ptr< sinks::ISink > > _sinks;
};

CLogger::CLogger() : _impl( std::make_unique< CLoggerImpl >() ) {}

CLogger::~CLogger() = default;

template < class... _Args >
void CLogger::info( std::string_view _fmt, _Args&&... _args ) const noexcept {
    this->_impl->info( _fmt, std::forward< _Args >( _args )... );
}

template < class... _Args >
void CLogger::trace( std::string_view _fmt, _Args&&... _args ) const noexcept {
    this->_impl->trace( _fmt, std::forward< _Args >( _args )... );
}

template < class... _Args >
void CLogger::debug( std::string_view _fmt, _Args&&... _args ) const noexcept {
    this->_impl->debug( _fmt, std::forward< _Args >( _args )... );
}

template < class... _Args >
void CLogger::warn( std::string_view _fmt, _Args&&... _args ) const noexcept {
    this->_impl->warn( _fmt, std::forward< _Args >( _args )... );
}

template < class... _Args >
void CLogger::error( std::string_view _fmt, _Args&&... _args ) const noexcept {
    this->_impl->error( _fmt, std::forward< _Args >( _args )... );
}

template < class... _Args >
void CLogger::fatal( std::string_view _fmt, _Args&&... _args ) const noexcept {
    this->_impl->fatal( _fmt, std::forward< _Args >( _args )... );
}

}  // namespace jzlog
