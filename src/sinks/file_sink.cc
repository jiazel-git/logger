#include "jzlog/sinks/file_sink.h"
#include "jzlog/archive_manager/archive_manager.h"
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/utils/fixed_buffer.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
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
    return tm_buf;
}
}  // anonymous namespace

CFileSink::CFileSink() noexcept :
    _level( LogLevel::TRACE ),
    _file_size( DEFAULT_FILE_SIZE ),
    _file_path( DEFAULT_FILE_PATH ),
    _cur_file_name( "" ),
    _cur_file_size( 0 ),
    _current_buffer( std::make_unique< Buffer >() ),
    _next_buffer( std::make_unique< Buffer >() ),
    _buffers(),
    _buffer_mutex(),
    _running( false ),
    _archive_manager( nullptr ) {

    if ( !_file_path.empty() && !std::filesystem::is_directory( _file_path ) ) {
        std::filesystem::create_directories( _file_path );
    }
    init_file_idx();
    create_new_file();
    start();
}

CFileSink::CFileSink( LogLevel lvl, uint32_t fsize, uint32_t buf_size, std::string path,
                      bool enable ) noexcept {}

CFileSink::CFileSink( LogLevel lvl, uint32_t fsize, uint32_t buf_size, bool enable,
                      const ArchiveConfig& archive_cfg ) noexcept :
    _level( lvl ),
    _file_size( fsize ),
    _file_path( archive_cfg.base_path + "/current" ),
    _cur_file_name( "" ),
    _cur_file_size( 0 ),
    _current_buffer( std::make_unique< Buffer >() ),
    _next_buffer( std::make_unique< Buffer >() ),
    _buffers(),
    _buffer_mutex(),
    _running( false ),
    _archive_manager( std::make_unique< CArchiveManager >( archive_cfg ) ) {
    (void)enable;

    if ( !_file_path.empty() && !std::filesystem::is_directory( _file_path ) ) {
        std::filesystem::create_directories( _file_path );
    }

    if ( _archive_manager && archive_cfg.enable_archive ) {
        _archive_manager->start();
    }

    init_file_idx();
    create_new_file();
    start();
}

bool CFileSink::write( const LogRecord& r ) noexcept {
    if ( !should_log( r._level ) || r._message.empty() ) {
        return false;
    }

    std::string format_record;
    try {
        format_record = format_log_record( r );
    } catch ( ... ) {
        return false;
    }

    {
        std::lock_guard< std::mutex > buffer_lock{ _buffer_mutex };
        if ( format_record.size() > _current_buffer->avail() ) {
            auto new_next = std::make_unique< Buffer >();
            try {
                _buffers.emplace_back( std::move( _current_buffer ) );
                _current_buffer = std::move( _next_buffer );
                _next_buffer    = std::move( new_next );
            } catch ( ... ) {
                return false;
            }
        }

        if ( format_record.size() > _current_buffer->avail() ) {
            return false;
        }

        _current_buffer->append( format_record.c_str(), format_record.size() );
    }

    _cond.notify_one();
    return true;
}

bool CFileSink::flush() noexcept {
    BufferVec write_buffers{};
    {
        std::lock_guard< std::mutex > lock{ _buffer_mutex };

        if ( _current_buffer->length() > 0 ) {
            _buffers.emplace_back( std::move( _current_buffer ) );
            _current_buffer.swap( _next_buffer );
            _next_buffer = std::make_unique< Buffer >();
        }

        write_buffers.swap( _buffers );
    }

    if ( write_buffers.empty() ) {
        return true;
    }

    bool all_success =
        std::all_of( write_buffers.begin(), write_buffers.end(), [ this ]( auto& buffer ) {
            return flush_buffer_to_file( std::move( buffer ) );
        } );
    return all_success;
}

void CFileSink::set_level( LogLevel lvl ) noexcept { _level = lvl; }

LogLevel CFileSink::level() const noexcept { return _level; }

bool CFileSink::should_log( LogLevel lvl ) const noexcept { return lvl >= _level; }

void CFileSink::create_new_file() noexcept {
    auto data_str = get_date_str();

    if ( data_str != _cur_date_str ) {
        _cur_date_str = data_str;
        _cur_idx      = 0;
    } else {
        ++_cur_idx;
    }

    std::stringstream ss_file_name;
    ss_file_name << _cur_date_str << "_" << std::setfill( '0' ) << std::setw( 3 ) << _cur_idx;
    _cur_file_name       = ss_file_name.str();

    std::string fullPath = _file_path + "/" + _cur_file_name;

    _file_stream.open( fullPath, std::ios::app );
    if ( _file_stream.is_open() ) {
        _cur_file_size = _file_stream.tellp();
    } else {
        std::cerr << "failed to create log file" << std::endl;
    }
}

bool CFileSink::flush_buffer_to_file( BufferPtr buffer_ptr ) noexcept {
    std::lock_guard< std::mutex > file_lock{ _file_mutex };

    if ( _cur_file_size + buffer_ptr->length() > _file_size ) {

        rotate_file_();
    }

    _file_stream.write( buffer_ptr->data(), buffer_ptr->length() );
    _cur_file_size += buffer_ptr->length();
    _file_stream.flush();

    return _file_stream.good();
}

void CFileSink::rotate_file_() {
    if ( _file_stream.is_open() ) {
        _file_stream.flush();
        _file_stream.close();
    }

    create_new_file();
}

std::string CFileSink::format_log_record( const LogRecord& r ) {
    std::stringstream ss;

    auto time = std::chrono::system_clock::to_time_t( r._timestamp );
    auto tm   = safe_localtime( time );  // 线程安全
    if ( !tm.has_value() ) {
        return "";
    }
    ss << std::put_time( &( tm.value() ), "%Y-%m-%d %H:%M:%S" ) << " ["
       << loglevel::to_string( r._level ) << "] " << "[" << r._thread_id << "]"
       << "[" << r._function << ":" << r._line << "]" << r._message << "\n";

    return ss.str();
}

void CFileSink::work_thread() noexcept {
    while ( _running ) {

        auto write_buffers = BufferVec{};

        {
            std::unique_lock< std::mutex > lock{ _buffer_mutex };

            _cond.wait_for( lock, std::chrono::seconds{ 3 }, [ this ]() {
                return !_buffers.empty() || !_running;
            } );

            if ( _current_buffer->length() > 0 ) {
                _buffers.emplace_back( std::move( _current_buffer ) );
                std::swap( _current_buffer, _next_buffer );
                _next_buffer = std::make_unique< Buffer >();
            }

            write_buffers.swap( _buffers );
        }

        if ( !write_buffers.empty() ) {
            std::for_each( write_buffers.begin(), write_buffers.end(), [ this ]( auto& buffer ) {
                flush_buffer_to_file( std::move( buffer ) );
            } );
        }
    }

    auto final_buffers = BufferVec{};

    {
        std::lock_guard< std::mutex > lock{ _buffer_mutex };
        if ( _current_buffer->length() > 0 ) {
            _buffers.emplace_back( std::move( _current_buffer ) );
            std::swap( _current_buffer, _next_buffer );
            _next_buffer = std::make_unique< Buffer >();
        }
        final_buffers.swap( _buffers );
    }

    if ( !final_buffers.empty() ) {
        std::for_each( final_buffers.begin(), final_buffers.end(), [ this ]( auto& buffer ) {
            flush_buffer_to_file( std::move( buffer ) );
        } );
    }
}

void CFileSink::start() noexcept {
    if ( !_running.exchange( true ) ) {
        _thread = std::thread( &CFileSink::work_thread, this );
    }
}

void CFileSink::init_file_idx() noexcept {
    std::string today_str = get_date_str();

    int max_idx           = -1;

    std::error_code ec;
    if ( !std::filesystem::exists( _file_path, ec ) ||
         !std::filesystem::is_directory( _file_path, ec ) ) {
        std::filesystem::create_directories( _file_path, ec );
        _cur_idx = 0;
        return;
    }

    for ( const auto& entry : std::filesystem::directory_iterator( _file_path, ec ) ) {
        if ( entry.is_regular_file() ) {
            std::string filename = entry.path().filename().string();

            if ( filename.length() > today_str.length() + 1 &&
                 filename.substr( 0, today_str.length() ) == today_str &&
                 filename[ today_str.length() ] == '_' ) {

                std::string idx_str = filename.substr( today_str.length() + 1 );

                try {
                    int current_idx = std::stoi( idx_str );
                    max_idx         = std::max( max_idx, current_idx );
                } catch ( ... ) {}
            }
        }
    }
    _cur_idx      = max_idx;

    _cur_date_str = today_str;
}

std::string CFileSink::get_date_str() noexcept {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t( now );
    auto tm   = safe_localtime( time );  // 线程安全
    if ( !tm.has_value() ) {
        return "";
    }
    std::stringstream ss;
    ss << std::put_time( &( tm.value() ), "%Y%m%d" );
    return ss.str();
}

void CFileSink::set_enabled( bool enabled ) noexcept { (void)enabled; }

bool CFileSink::enabled() const noexcept { return true; }

void CFileSink::enable_archive( bool enable ) noexcept {
    if ( _archive_manager ) {
        if ( enable ) {
            _archive_manager->start();
        } else {
            _archive_manager->stop();
        }
    }
}

CFileSink::~CFileSink() {
    try {
        if ( _running.exchange( false ) ) {
            _cond.notify_all();
            if ( _thread.joinable() ) {
                _thread.join();
            }
        }
        if ( _archive_manager ) {
            _archive_manager->stop();
        }
        flush();
        if ( _file_stream.is_open() ) {
            _file_stream.close();
        }
    } catch ( ... ) {
        std::cerr << "Error in CFileSink destructor" << std::endl;
    }
}

}  // namespace sinks
}  // namespace jzlog
