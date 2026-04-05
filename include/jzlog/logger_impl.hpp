/**
 * @file logger_impl.hpp
 * @brief 日志记录器实现类
 */
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
 */
class CLoggerImpl {
public:
    /**
     * @brief 构造函数
     * @param lvl 日志级别
     * @param logger_name 日志记录器名称
     */
    explicit CLoggerImpl( loglevel::LogLevel lvl, std::string_view logger_name ) noexcept {
        try {
            (void)lvl;
            (void)logger_name;
        } catch (...) {
        }
    }

    /**
     * @brief 拷贝构造函数（已删除）
     */
    CLoggerImpl( const CLoggerImpl& )            = delete;

    /**
     * @brief 拷贝赋值运算符（已删除）
     */
    CLoggerImpl& operator=( const CLoggerImpl& ) = delete;

    /**
     * @brief 移动构造函数（已删除）
     */
    CLoggerImpl( CLoggerImpl&& oth )             = delete;

    /**
     * @brief 移动赋值运算符（已删除）
     */
    CLoggerImpl& operator=( CLoggerImpl&& oth )  = delete;

    /**
     * @brief 析构函数
     */
    ~CLoggerImpl()                               = default;

public:
    /**
     * @brief 记录 INFO 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    inline bool info( std::string_view fmt, Args&&... args ) const {
        return add_record( LogLevel ::INFO, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 TRACE 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    inline bool trace( std::string_view fmt, Args&&... args ) const {
        return add_record( LogLevel::TRACE, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 DEBUG 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    inline bool debug( std::string_view fmt, Args&&... args ) const {
        return add_record( LogLevel::DEBUG, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 WARN 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    inline bool warn( std::string_view fmt, Args&&... args ) const {
        return add_record( LogLevel::WARN, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 ERROR 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    inline bool error( std::string_view fmt, Args&&... args ) const {
        return add_record( LogLevel::ERROR, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 FATAL 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    inline bool fatal( std::string_view fmt, Args&&... args ) const {
        return add_record( LogLevel::FATAL, fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 添加日志输出目标
     * @param sink 日志输出目标的智能指针
     * @return 成功返回 true，失败返回 false
     */
    bool add_sink( std::unique_ptr< sinks::ISink > sink ) {
        if ( !sink ) {
            return false;
        }
        _sinks.emplace_back( std::move( sink ) );
        return true;
    }

private:
    /**
     * @brief 将日志记录写入所有输出目标
     * @param record 日志记录
     * @return 所有输出目标都成功返回 true，否则返回 false
     */
    bool log( LogRecord&& record ) const {
        bool all_success = true;
        for ( auto& sink : _sinks ) {
            if ( !sink->write( record ) ) {
                all_success = false;
            }
        }
        return all_success;
    }

    /**
     * @brief 添加日志记录
     * @tparam Args 可变模板参数类型
     * @param level 日志级别
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    bool add_record( LogLevel level, std::string_view fmt, Args&&... args ) const {
        int required = snprintf( nullptr, 0, fmt.data(), std::forward< Args >( args )... );

        if ( required < 0 ) {
            return false;
        }
        
        std::vector< char > buf;
        LogRecord record;
        
        try {
            buf.resize( required + 1, 0 );
            snprintf( buf.data(), required + 1, fmt.data(), std::forward< Args >( args )... );
            
            record._function  = __func__;
            record._line      = __LINE__;
            record._thread_id = std::this_thread::get_id();
            record._message   = std::string( buf.data(), required );
            record._level     = level;
            
            return log( std::move( record ) );
        } catch ( ... ) {
            return false;
        }
    }

private:
    std::vector< std::unique_ptr< sinks::ISink > > _sinks;  // 日志输出目标列表
};

}  // namespace jzlog
