#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/slinks/file_sink.h"
#include "jzlog/utils/log_buffer.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>

namespace jzlog
{
namespace sinks
{
CFileSink::CFileSink() noexcept :
    _M_Base(),
    _level( LogLevel::TRACE ),
    _file_size( 10 * 1024 * 1024 ),
    _buffer_size( 8 * 1024 ),
    _file_path( "../log/" ),
    _cur_file_name( "" ),
    _cur_file_size( 0 ),
    _enabled( true ),
    _buffer( _buffer_size ) {
    create_new_file();
}

CFileSink::CFileSink( LogLevel _lvl, uint32_t _fsize, uint32_t _buf_size, std::string _path,
                      std::string _fname, bool _enable ) noexcept :
    _M_Base(),
    _level( _lvl ),
    _file_size( _fsize ),
    _buffer_size( _buf_size ),
    _file_path( _path ),
    _cur_file_name( _fname ),
    _cur_file_size( 0 ),
    _enabled( _enable ),
    _buffer( _buffer_size ) {
    create_new_file();
}

void CFileSink::write( const LogRecord& _r ) {
    if ( !enabled() || !should_log( _r._level ) ) {
        return;
    }

    // 格式化日志记录
    std::string fmt_record = format_log_record( _r );

    if ( _buffer._size + fmt_record.size() > _buffer_size ) {
        flush_buffer_to_file();
    }

    if ( _cur_file_size + _buffer._size + fmt_record.size() > _file_size ) {
        rotate_file();
    }

    std::memcpy( _buffer._buf.get() + _buffer._pos, fmt_record.data(), fmt_record.size() );
    std::cout << "record:" << fmt_record;
    _buffer._size += fmt_record.size();
    _buffer._pos += fmt_record.size();
    std::cout << "buf:" << _buffer._buf.get() << std::endl;

    // if ( _buffer._size > _buffer_size * 0.8 ) {
    flush_buffer_to_file();
    //}
}

void CFileSink::flush() {
    flush_buffer_to_file();
    if ( _file_stream ) {
        _file_stream.flush();
    }
}

void CFileSink::set_level( LogLevel _lvl ) noexcept { _level = _lvl; }

LogLevel CFileSink::level() const noexcept { return _level; }

bool CFileSink::should_log( LogLevel _lvl ) const noexcept { return _lvl >= _level; }

void CFileSink::set_enabled( bool _enable ) noexcept { _enabled = _enable; }

bool CFileSink::enabled() const noexcept { return _enabled; }

void CFileSink::create_new_file() {
    // 确保目录存在
    if ( !_file_path.empty() ) {
        std::filesystem::create_directories( _file_path );
    }

    // 生成带时间戳的文件名
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t( now );

    std::stringstream ss;
    ss << std::put_time( std::localtime( &time ), "%Y%m%d_%H%M%S" );

    _cur_file_name = _file_path + "/" + _cur_file_name + "_" + ss.str() + ".log";

    // 打开文件
    _file_stream.open( _cur_file_name, std::ios::app );
    _cur_file_size = _file_stream.tellp();
}

void CFileSink::flush_buffer_to_file() {
    if ( _buffer._size > 0 && _file_stream.is_open() ) {
        _file_stream.write( _buffer._buf.get(), _buffer._size );
        _cur_file_size += _buffer._size;
        _buffer.clear_buf();
        flush();
    }
}

void CFileSink::rotate_file() {
    flush_buffer_to_file();
    _file_stream.close();
    create_new_file();
}

std::string CFileSink::format_log_record( LogRecord _r ) {
    // 简单的格式化示例，可根据需求调整
    std::stringstream ss;

    auto time = std::chrono::system_clock::to_time_t( _r._timestamp );

    ss << std::put_time( std::localtime( &time ), "%Y-%m-%d %H:%M:%S" ) << " ["
       << loglevel::to_string( _r._level ) << "] " << _r._message << "\n";

    return ss.str();
}

CFileSink::~CFileSink() { flush_buffer_to_file(); }
}  // namespace sinks
}  // namespace jzlog
