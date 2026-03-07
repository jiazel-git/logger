#pragma once
#include "jzlog/archive_manager/archive_manager.h"
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/utils/log_buffer.h"
#include "sink.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
namespace jzlog
{
namespace sinks
{
using namespace loglevel;
class CFileSink final : public ISink {

public:
    using _M_Base = ISink;

public:
    /**
     * @brief 默认构造函数
     * @note 使用默认配置，不启用日志归档功能
     */
    explicit CFileSink() noexcept;

    /**
     * @brief 构造函数：自定义配置
     * @param _lvl 日志级别
     * @param _fsize 单个日志文件最大大小（字节）
     * @param _buf_size 缓冲区大小（字节）
     * @param _dir 日志文件输出目录
     * @param _enable 是否启用日志输出
     * @note 此构造函数不启用日志归档功能
     */
    explicit CFileSink( LogLevel lvl, uint32_t fsize, uint32_t buf_size, std::string dir,
                        bool enable ) noexcept;

    /**
     * @brief 构造函数：启用日志归档功能
     * @param _lvl 日志级别
     * @param _fsize 单个日志文件最大大小（字节）
     * @param _buf_size 缓冲区大小（字节）
     * @param _enable 是否启用日志输出
     * @param _archive_cfg 归档配置参数
     * @note 使用此构造函数会自动启动后台归档线程
     */
    explicit CFileSink( LogLevel lvl, uint32_t fsize, uint32_t buf_size, bool enable,
                        const ArchiveConfig& archive_cfg ) noexcept;

    // 禁止拷贝和移动
    CFileSink( const CFileSink& )            = delete;
    CFileSink& operator=( const CFileSink& ) = delete;
    CFileSink( CFileSink&& )                 = delete;
    CFileSink& operator=( CFileSink&& )      = delete;

    void write( const LogRecord& r ) override;

    void flush() override;

    void set_level( LogLevel lvl ) noexcept override;

    LogLevel level() const noexcept override;

    bool should_log( LogLevel lvl ) const noexcept override;

    void set_enabled( bool enable ) noexcept override;

    bool enabled() const noexcept override;

    ~CFileSink();

    /**
     * @brief 启用或禁用日志归档功能
     * @param enable true 启用归档，false 禁用归档
     * @note 可以在运行时动态启用或禁用归档功能
     */
    void enable_archive( bool enable ) noexcept;

    /**
     * @brief 获取归档管理器指针
     * @return 归档管理器指针，如果未启用归档则返回 nullptr
     * @note 用于高级配置，如动态修改归档参数
     */
    CArchiveManager* get_archive_manager() noexcept { return _archive_manager.get(); }

private:
    void create_buffers();

    void create_new_file();

    std::string format_log_record( LogRecord r );

    void flush_buffer_to_file();

    void rotate_file();

    void process_expired_file();

private:
    LogLevel      _level;          ///< 日志级别过滤
    uint32_t      _file_size;      ///< 单个日志文件最大大小（字节）
    uint32_t      _buffer_size;    ///< 缓冲区大小（字节）
    std::string   _file_path;      ///< 日志文件输出目录
    std::string   _cur_file_name;  ///< 当前日志文件名
    uint32_t      _cur_file_size;     ///< 当前日志文件大小（字节）
    bool          _enabled;           ///< 是否启用日志输出
    std::array<Buffer, 2> _buffers;   ///< 双缓冲区
    bool          _write_buf_0{true}; ///< true=写buffer[0], false=写buffer[1]
    std::mutex    _swap_mutex;        ///< 保护缓冲区交换
    std::ofstream _file_stream;       ///< 文件输出流

    // ========== 归档管理相关成员 ==========
    std::unique_ptr< CArchiveManager > _archive_manager;  ///< 归档管理器（智能指针管理）
    ArchiveConfig                      _archive_config;   ///< 归档配置参数
    bool                               _enable_archive;   ///< 是否启用归档功能
};
}  // namespace sinks
}  // namespace jzlog
