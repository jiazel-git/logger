#pragma once
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/sinks/sink.h"
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace jzlog
{

/**
 * @class CLoggerImpl
 * @brief 日志记录器实现类
 * @details 采用简化架构：用户线程直接调用 sink.write()，由 sink 内部处理缓冲
 *
 * 架构对比：
 * - 旧架构：用户线程 → 队列 → 工作线程 → sink → 双缓冲区 → 磁盘
 * - 新架构：用户线程 → sink → 三缓冲区 → 磁盘
 *
 * 优势：
 * - 减少一次线程间拷贝
 * - 降低延迟
 * - 简化代码复杂度
 */
class CLoggerImpl {
public:
    /**
     * @brief 构造函数
     * @param lvl 日志级别
     * @param logger_name 日志器名称
     */
    explicit CLoggerImpl( loglevel::LogLevel lvl, std::string_view logger_name ) noexcept {}

    CLoggerImpl( const CLoggerImpl& )            = delete;
    CLoggerImpl& operator=( const CLoggerImpl& ) = delete;

    CLoggerImpl( CLoggerImpl&& oth )             = delete;
    CLoggerImpl& operator=( CLoggerImpl&& oth )  = delete;

    ~CLoggerImpl()                               = default;

public:
    /**
     * @brief 记录 INFO 级别日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template < class... Args >
    inline void info( std::string_view fmt, Args&&... args ) const noexcept {
        add_record( LogLevel ::INFO, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 TRACE 级别日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template < class... Args >
    inline void trace( std::string_view fmt, Args&&... args ) const noexcept {
        add_record( LogLevel::TRACE, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 DEBUG 级别日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template < class... Args >
    inline void debug( std::string_view fmt, Args&&... args ) const noexcept {
        add_record( LogLevel::DEBUG, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 WARN 级别日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template < class... Args >
    inline void warn( std::string_view fmt, Args&&... args ) const noexcept {
        add_record( LogLevel::WARN, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 ERROR 级别日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template < class... Args >
    inline void error( std::string_view fmt, Args&&... args ) const noexcept {
        add_record( LogLevel::ERROR, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 FATAL 级别日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template < class... Args >
    inline void fatal( std::string_view fmt, Args&&... args ) const noexcept {
        add_record( LogLevel::FATAL, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 添加日志输出目标
     * @param sink 日志 sink 智能指针
     */
    void add_sink( std::unique_ptr< sinks::ISink > sink ) noexcept {
        _sinks.emplace_back( std::move( sink ) );
    }

private:
    /**
     * @brief 将日志记录写入所有 sink
     * @param record 日志记录
     */
    void log( LogRecord&& record ) const noexcept {
        for ( auto& sink : _sinks ) {
            sink->write( record );
        }
    }

    /**
     * @brief 构造并记录日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 格式化参数
     */
    template < class... Args >
    void add_record( LogLevel level, std::string_view fmt, Args&&... args ) const noexcept {
        int required = snprintf( nullptr, 0, fmt.data(), std::forward< Args >( args )... );

        if ( required < 0 ) {
            return;
        }
        std::vector< char > buf( required + 1, 0 );

        snprintf( buf.data(), required + 1, fmt.data(), std::forward< Args >( args )... );
        LogRecord record;
        record._function  = __func__;
        record._line      = __LINE__;
        record._thread_id = std::this_thread::get_id();
        record._message   = std::string( buf.data(), required );
        record._level     = level;
        log( std::move( record ) );
    }

private:
    std::vector< std::unique_ptr< sinks::ISink > > _sinks;
};

}  // namespace jzlog
