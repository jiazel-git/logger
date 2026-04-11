#include "jzlog/sinks/network_sink.h"
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include <algorithm>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace jzlog
{
namespace sinks
{

namespace
{
std::optional< std::tm > safe_localtime( std::time_t time ) {
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s( &tm_buf, &time );
#else
    localtime_r( &time, &tm_buf );
#endif
    return std::make_optional( tm_buf );
}
}  // anonymous namespace

CNetworkSink::CNetworkSink() noexcept :
    _level( LogLevel::TRACE ),
    _host( DEFAULT_HOST ),
    _port( DEFAULT_PORT ),
    _socket_fd( -1 ),
    _connected( false ),
    _batch_size( DEFAULT_BATCH_SIZE ),
    _batch_timeout_ms( DEFAULT_BATCH_TIMEOUT_MS ),
    _batch_buffer(),
    _buffer_mutex(),
    _retry_interval_ms( DEFAULT_RETRY_INTERVAL_MS ),
    _retry_backoff( 1 ),
    _running( false ) {
    start();
}

CNetworkSink::CNetworkSink( std::string host, uint16_t port, LogLevel level, size_t batch_size,
                            uint32_t batch_timeout_ms, uint32_t retry_interval_ms,
                            bool enable ) noexcept :
    _level( level ),
    _host( std::move( host ) ),
    _port( port ),
    _socket_fd( -1 ),
    _connected( false ),
    _batch_size( batch_size ),
    _batch_timeout_ms( batch_timeout_ms ),
    _batch_buffer(),
    _buffer_mutex(),
    _retry_interval_ms( retry_interval_ms ),
    _retry_backoff( 1 ),
    _running( false ) {
    (void)enable;
    start();
}

bool CNetworkSink::write( const LogRecord& r ) noexcept {
    std::cout << r._message << std::endl;
    if ( !should_log( r._level ) || r._message.empty() ) {
        return false;
    }

    {
        std::lock_guard lock{ _buffer_mutex };
        _batch_buffer.push_back( r );
    }
    std::cout << "write success" << std::endl;

    _cond.notify_one();
    return true;
}

bool CNetworkSink::flush() noexcept {
    bool result = true;
    {
        std::lock_guard lock{ _buffer_mutex };
        if ( !_batch_buffer.empty() ) {
            result = send_batch();
        }
    }
    return result;
}

void CNetworkSink::set_level( LogLevel lvl ) noexcept { _level = lvl; }

LogLevel CNetworkSink::level() const noexcept { return _level; }

bool CNetworkSink::should_log( LogLevel lvl ) const noexcept { return lvl >= _level; }

void CNetworkSink::set_enabled( bool enabled ) noexcept { _enabled.store( enabled ); }

bool CNetworkSink::enabled() const noexcept { return _enabled.load(); }

void CNetworkSink::start() noexcept {
    if ( !_running.exchange( true ) ) {
        _thread = std::thread( &CNetworkSink::work_thread, this );
    }
}

bool CNetworkSink::connect() noexcept {
    if ( _socket_fd >= 0 ) {
        return true;
    }

    _socket_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( _socket_fd < 0 ) {
        std::cerr << "Failed to create socket: " << strerror( errno ) << std::endl;
        return false;
    }

    if ( !set_socket_timeout( SOCKET_SEND_TIMEOUT_MS ) ) {
        close( _socket_fd );
        _socket_fd = -1;
        return false;
    }

    struct sockaddr_in server_addr;
    std::memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons( _port );

    if ( inet_pton( AF_INET, _host.c_str(), &server_addr.sin_addr ) <= 0 ) {
        struct hostent* hostent = gethostbyname( _host.c_str() );
        if ( hostent == nullptr ) {
            std::cerr << "Failed to resolve host: " << _host << std::endl;
            close( _socket_fd );
            _socket_fd = -1;
            return false;
        }
        std::memcpy( &server_addr.sin_addr, hostent->h_addr, hostent->h_length );
    }

    if ( ::connect( _socket_fd, reinterpret_cast< struct sockaddr* >( &server_addr ),
                    sizeof( server_addr ) ) < 0 ) {
        std::cerr << "Failed to connect to " << _host << ":" << _port << ": " << strerror( errno )
                  << std::endl;
        close( _socket_fd );
        _socket_fd = -1;
        return false;
    }
    std::cout << "Connect success" << std::endl;

    _connected     = true;
    _retry_backoff = 1;
    return true;
}

void CNetworkSink::disconnect() noexcept {
    if ( _socket_fd >= 0 ) {
        close( _socket_fd );
        _socket_fd = -1;
    }
    _connected = false;
}

void CNetworkSink::auto_reconnect() noexcept {
    disconnect();

    uint32_t retry_interval = _retry_interval_ms * _retry_backoff;
    if ( retry_interval > MAX_RETRY_INTERVAL_MS ) {
        retry_interval = MAX_RETRY_INTERVAL_MS;
    }

    std::this_thread::sleep_for( std::chrono::milliseconds( retry_interval ) );

    if ( _retry_backoff < 32 ) {
        _retry_backoff *= 2;
    }

    if ( connect() ) {
        std::cerr << "Reconnected to " << _host << ":" << _port << std::endl;
    }
}

bool CNetworkSink::send_batch() noexcept {
    if ( _batch_buffer.empty() ) {
        return true;
    }

    if ( !connect() ) {
        auto_reconnect();
        return false;
    }

    std::stringstream ss;
    std::for_each( _batch_buffer.begin(), _batch_buffer.end(),
                   [ &ss, this ]( const LogRecord& record ) {
                       ss << format_log_record( record );
                   } );

    std::string data = ss.str();
    if ( data.empty() ) {
        _batch_buffer.clear();
        return true;
    }

    ssize_t total_sent = 0;
    while ( total_sent < static_cast< ssize_t >( data.size() ) ) {
        ssize_t sent =
            send( _socket_fd, data.data() + total_sent, data.size() - total_sent, MSG_NOSIGNAL );
        if ( sent < 0 ) {
            if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
                std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
                continue;
            }
            std::cerr << "Failed to send data: " << strerror( errno ) << std::endl;
            auto_reconnect();
            _batch_buffer.clear();
            return false;
        }
        total_sent += sent;
    }

    _batch_buffer.clear();
    return true;
}

void CNetworkSink::work_thread() noexcept {
    while ( _running ) {
        std::unique_lock lock{ _buffer_mutex };

        _cond.wait_for( lock, std::chrono::milliseconds( _batch_timeout_ms ), [ this ]() {
            return _batch_buffer.size() >= _batch_size || !_running;
        } );

        if ( _batch_buffer.size() >= _batch_size ) {
            send_batch();
        }
    }

    flush();
}

std::string CNetworkSink::format_log_record( const LogRecord& r ) {
    std::stringstream ss;

    auto time = std::chrono::system_clock::to_time_t( r._timestamp );
    auto tm   = safe_localtime( time );
    if ( !tm.has_value() ) {
        return "";
    }
    ss << std::put_time( &( tm.value() ), "%Y-%m-%d %H:%M:%S" ) << " ["
       << loglevel::to_string( r._level ) << "] " << "[" << r._thread_id << "]"
       << "[" << r._function << ":" << r._line << "]" << r._message << "\n";

    return ss.str();
}

bool CNetworkSink::set_socket_timeout( uint32_t timeout_ms ) noexcept {
    if ( _socket_fd < 0 ) {
        return false;
    }

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = ( timeout_ms % 1000 ) * 1000;

    if ( setsockopt( _socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof( tv ) ) < 0 ) {
        std::cerr << "Failed to set socket send timeout: " << strerror( errno ) << std::endl;
        return false;
    }

    if ( setsockopt( _socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) ) < 0 ) {
        std::cerr << "Failed to set socket receive timeout: " << strerror( errno ) << std::endl;
        return false;
    }

    int buffer_size = DEFAULT_SOCKET_BUFFER_SIZE;
    if ( setsockopt( _socket_fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof( buffer_size ) ) <
         0 ) {
        std::cerr << "Failed to set socket send buffer size: " << strerror( errno ) << std::endl;
    }

    return true;
}

CNetworkSink::~CNetworkSink() {
    try {
        if ( _running.exchange( false ) ) {
            _cond.notify_all();
            if ( _thread.joinable() ) {
                _thread.join();
            }
        }
        flush();
        disconnect();
    } catch ( ... ) {
        std::cerr << "Error in CNetworkSink destructor" << std::endl;
    }
}

}  // namespace sinks
}  // namespace jzlog
