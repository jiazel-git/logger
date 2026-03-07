#include "jzlog/sinks/file_sink.h"
#include "jzlog/archive_manager/archive_manager.h"
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/utils/log_buffer.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

namespace jzlog
{
namespace sinks
{
/**
 * @brief 默认构造函数
 * @note 使用默认配置，不启用日志归档功能
 */
CFileSink::CFileSink() noexcept :
    _M_Base(),
    _level( LogLevel::TRACE ),
    _file_size( 10 * 1024 * 1024 ),
    _buffer_size( 4 * 1024 ),
    _file_path( "../log/" ),
    _cur_file_name( "" ),
    _cur_file_size( 0 ),
    _enabled( true ),
    _buffers{ Buffer( _buffer_size ), Buffer( _buffer_size ) },
    _write_buf_0( true ),
    _swap_mutex(),
    _archive_manager(),
    _archive_config(),
    _enable_archive( false ) {
    create_new_file();
}

/**
 * @brief 构造函数：自定义配置
 * @note 不启用日志归档功能
 */
CFileSink::CFileSink( LogLevel lvl, uint32_t fsize, uint32_t buf_size, std::string path,
                      bool enable ) noexcept :
    _M_Base(),
    _level( lvl ),
    _file_size( fsize ),
    _buffer_size( buf_size ),
    _file_path( path ),
    _cur_file_name( "" ),
    _cur_file_size( 0 ),
    _enabled( enable ),
    _buffers{ Buffer( _buffer_size ), Buffer( _buffer_size ) },
    _write_buf_0( true ),
    _swap_mutex(),
    _archive_manager(),
    _archive_config(),
    _enable_archive( false ) {
    create_new_file();
}

/**
 * @brief 构造函数：启用日志归档功能
 * @param _archive_cfg 归档配置参数
 *
 * 构造流程：
 * 1. 初始化基本成员变量
 * 2. 将日志路径设置为 base_path/current/
 * 3. 创建归档管理器并启动后台线程
 * 4. 创建第一个日志文件
 */
CFileSink::CFileSink( LogLevel lvl, uint32_t fsize, uint32_t buf_size, bool enable,
                      const ArchiveConfig& archive_cfg ) noexcept :
    _M_Base(),
    _level( lvl ),
    _file_size( fsize ),
    _buffer_size( buf_size ),
    _file_path( archive_cfg.base_path + "/current" ),  // 使用 /current 子目录存储当前日志
    _cur_file_name( "" ),
    _cur_file_size( 0 ),
    _enabled( enable ),
    _buffers{ Buffer( _buffer_size ), Buffer( _buffer_size ) },
    _write_buf_0( true ),
    _swap_mutex(),
    _archive_config( archive_cfg ),
    _enable_archive( true ) {
    // 创建并启动归档管理器
    _archive_manager = std::make_unique< CArchiveManager >( archive_cfg );
    _archive_manager->start();
    create_new_file();
}

void CFileSink::write( const LogRecord& r ) {
    if ( !enabled() || !should_log( r._level ) ) {
        return;
    }

    std::string fmt_record = format_log_record( r );

    auto& write_buf        = _buffers[ _write_buf_0 ? 0 : 1 ];

    if ( write_buf._size + fmt_record.size() > _buffer_size ) {
        flush_buffer_to_file();
    }

    if ( _cur_file_size + _buffers[ _write_buf_0 ? 0 : 1 ]._size + fmt_record.size() >
         _file_size ) {
        rotate_file();
    }

    auto& buf = _buffers[ _write_buf_0 ? 0 : 1 ];
    std::memcpy( buf._buf.get() + buf._pos, fmt_record.data(), fmt_record.size() );
    buf._size += fmt_record.size();
    buf._pos += fmt_record.size();

    if ( _buffers[ _write_buf_0 ? 0 : 1 ]._size > _buffer_size * 0.8 ) {
        flush_buffer_to_file();
    }
}

void CFileSink::flush() { flush_buffer_to_file(); }

void CFileSink::set_level( LogLevel lvl ) noexcept { _level = lvl; }

LogLevel CFileSink::level() const noexcept { return _level; }

bool CFileSink::should_log( LogLevel lvl ) const noexcept { return lvl >= _level; }

void CFileSink::set_enabled( bool enable ) noexcept { _enabled = enable; }

bool CFileSink::enabled() const noexcept { return _enabled; }

void CFileSink::create_new_file() {
    if ( !_file_path.empty() ) {
        std::filesystem::create_directories( _file_path );
    }

    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t( now );

    std::stringstream ss;
    ss << std::put_time( std::localtime( &time ), "%Y%m%d_%H%M%S" );

    _cur_file_name = _file_path + "/" + _cur_file_name + "_" + ss.str() + ".log";

    _file_stream.open( _cur_file_name, std::ios::app );
    _cur_file_size = _file_stream.tellp();
}

void CFileSink::flush_buffer_to_file() {
    // 交换缓冲区
    {
        std::lock_guard< std::mutex > lock( _swap_mutex );
        _write_buf_0 = !_write_buf_0;  // 翻转索引
    }

    // 获取读缓冲区（刚才写完的那个）
    auto& read_buf = _buffers[ _write_buf_0 ? 1 : 0 ];

    // 异步写入读缓冲区内容到文件
    if ( read_buf._size > 0 && _file_stream.is_open() ) {
        _file_stream.write( read_buf._buf.get(), read_buf._size );
        _cur_file_size += read_buf._size;
        read_buf.clear_buf();
        _file_stream.flush();
    }
}

/**
 * @brief 文件轮转：当当前文件大小超过限制时创建新文件
 * @details 执行流程：
 * 1. 刷新缓冲区内容到文件
 * 2. 关闭当前文件流
 * 3. 通知归档管理器处理已关闭的文件（如果启用了归档）
 * 4. 创建新的日志文件
 */
void CFileSink::rotate_file() {
    flush_buffer_to_file();
    _file_stream.close();
    create_new_file();
}

std::string CFileSink::format_log_record( LogRecord r ) {
    std::stringstream ss;

    auto time = std::chrono::system_clock::to_time_t( r._timestamp );

    ss << std::put_time( std::localtime( &time ), "%Y-%m-%d %H:%M:%S" ) << " ["
       << loglevel::to_string( r._level ) << "] " << r._message << "\n";

    return ss.str();
}

/**
 * @brief 启用或禁用日志归档功能
 * @param enable true 启用归档，false 禁用归档
 * @note 支持运行时动态启用/禁用，会相应启动或停止归档后台线程
 */
void CFileSink::enable_archive( bool enable ) noexcept {
    if ( enable && !_enable_archive && _archive_manager ) {
        // 启动归档管理器
        _archive_manager->start();
    } else if ( !enable && _enable_archive && _archive_manager ) {
        // 停止归档管理器
        _archive_manager->stop();
    }

    _enable_archive = enable;
}

/**
 * @brief 析构函数：确保资源正确释放
 * @details 执行流程：
 * 1. 刷新缓冲区内容到文件
 * 2. 停止归档管理器（如果启用）
 */
CFileSink::~CFileSink() {
    flush_buffer_to_file();

    // 停止归档管理器（如果存在）
    if ( _archive_manager ) {
        _archive_manager->stop();
    }
}
}  // namespace sinks
}  // namespace jzlog
