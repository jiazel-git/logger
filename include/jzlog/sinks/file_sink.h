/**
 * @file file_sink.h
 * @brief 文件日志 Sink 实现类
 */
#pragma once
#include "jzlog/archive_manager/archive_manager.h"
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/utils/fixed_buffer.h"
#include "sink.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
namespace jzlog
{
namespace sinks
{
using namespace loglevel;

constexpr int              DEFAULT_FILE_SIZE{ 100 * 1024 * 1024 };
constexpr std::string_view DEFAULT_FILE_PATH{ "/home/carbon/workspace/logger/log" };

/**
 * @class CFileSink
 * @brief 文件日志 Sink 实现类，支持日志文件滚动和缓冲
 */
class CFileSink final : public ISink {
public:
    using Buffer    = utils::FixedBuffer< utils::kLargeBuffer >;  // 缓冲区类型
    using BufferPtr = std::unique_ptr< Buffer >;                  // 缓冲区指针类型
    using BufferVec = std::vector< BufferPtr >;                   // 缓冲区向量类型

public:
    /**
     * @brief 默认构造函数
     */
    explicit CFileSink() noexcept;

    /**
     * @brief 构造函数（带目录路径）
     * @param level 日志级别
     * @param fileSize 单个日志文件最大大小
     * @param bufSize 缓冲区大小
     * @param dir 日志文件存储目录
     * @param enable 是否启用
     */
    explicit CFileSink( LogLevel level, uint32_t fileSize, uint32_t bufSize, std::string dir,
                        bool enable ) noexcept;

    /**
     * @brief 构造函数（带归档配置）
     * @param level 日志级别
     * @param fileSize 单个日志文件最大大小
     * @param bufSize 缓冲区大小
     * @param enable 是否启用
     * @param archiveConfig 归档配置
     */
    explicit CFileSink( LogLevel level, uint32_t fileSize, uint32_t bufSize, bool enable,
                        const ArchiveConfig& archiveConfig ) noexcept;

    /**
     * @brief 拷贝构造函数（已删除）
     */
    CFileSink( const CFileSink& ) = delete;

    /**
     * @brief 拷贝赋值运算符（已删除）
     */
    CFileSink& operator=( const CFileSink& ) = delete;

    /**
     * @brief 移动构造函数（已删除）
     */
    CFileSink( CFileSink&& ) = delete;

    /**
     * @brief 移动赋值运算符（已删除）
     */
    CFileSink& operator=( CFileSink&& ) = delete;

    /**
     * @brief 写入日志记录
     * @param r 日志记录
     * @return 成功返回 true，失败返回 false
     */
    bool write( const LogRecord& r ) noexcept override;

    /**
     * @brief 刷新缓冲区
     * @return 成功返回 true，失败返回 false
     */
    bool flush() noexcept override;

    /**
     * @brief 设置日志级别
     * @param lvl 日志级别
     */
    void set_level( LogLevel lvl ) noexcept override;

    /**
     * @brief 获取日志级别
     * @return 当前日志级别
     */
    LogLevel level() const noexcept override;

    /**
     * @brief 判断是否应该记录该级别的日志
     * @param lvl 日志级别
     * @return 应该记录返回 true，否则返回 false
     */
    bool should_log( LogLevel lvl ) const noexcept override;

    /**
     * @brief 设置是否启用
     * @param enable 启用状态
     */
    void set_enabled( bool enable ) noexcept override;

    /**
     * @brief 获取启用状态
     * @return 启用返回 true，否则返回 false
     */
    bool enabled() const noexcept override;

    /**
     * @brief 析构函数
     */
    ~CFileSink();

    /**
     * @brief 启用/禁用归档功能
     * @param enable 是否启用归档
     */
    void enable_archive( bool enable ) noexcept;

private:
    /**
     * @brief 创建新的日志文件
     */
    void create_new_file() noexcept;

    /**
     * @brief 格式化日志记录
     * @param r 日志记录
     * @return 格式化后的字符串
     */
    std::string format_log_record( const LogRecord& r );

    /**
     * @brief 将缓冲区内容刷新到文件
     * @param buffer 缓冲区指针
     * @return 成功返回 true，失败返回 false
     */
    bool flush_buffer_to_file( BufferPtr buffer ) noexcept;

    /**
     * @brief 滚动日志文件
     */
    void rotate_file();

    /**
     * @brief 滚动日志文件（内部实现）
     */
    void rotate_file_();

    /**
     * @brief 处理过期文件
     */
    void process_expired_file();

    /**
     * @brief 启动后台工作线程
     */
    void start() noexcept;

    /**
     * @brief 后台工作线程主函数
     */
    void work_thread() noexcept;

    /**
     * @brief 初始化文件索引
     */
    void init_file_idx() noexcept;

    /**
     * @brief 获取当前日期字符串
     * @return 日期字符串（格式：YYYYMMDD）
     */
    std::string get_date_str() noexcept;

private:
    LogLevel                _level;            // 日志过滤级别
    uint32_t                _file_size;        // 单个日志文件最大大小
    std::string             _file_path;        // 日志文件存储目录路径
    std::string             _cur_file_name;    // 当前日志文件名
    uint32_t                _cur_file_size;    // 当前日志文件已写入大小
    BufferPtr               _current_buffer;   // 当前写入缓冲区
    BufferPtr               _next_buffer;      // 备用缓冲区
    BufferVec               _buffers;          // 待写入的缓冲区队列
    std::mutex              _buffer_mutex;     // 缓冲区互斥锁
    std::condition_variable _cond;             // 条件变量，用于工作线程
    std::ofstream           _file_stream;      // 文件输出流
    int                     _cur_idx;          // 当前文件索引
    std::string             _cur_date_str;     // 当前日期字符串
    std::thread             _thread;           // 后台工作线程
    std::atomic< bool >     _running;          // 线程运行标志
    std::atomic< bool >     _enabled{ true };  // Sink 启用状态
    std::mutex              _file_mutex;       // 文件操作互斥锁
};
}  // namespace sinks
}  // namespace jzlog
