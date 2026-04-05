/**
 * @file logger.hpp
 * @brief 日志记录器类
 */
#pragma once

#include "jzlog/sinks/sink.h"
#include "logger_impl.hpp"
#include <memory>
#include <string_view>
#include <utility>
namespace jzlog
{

/**
 * @class CLogger
 * @brief 日志记录器类，提供日志记录功能
 *
 * CLogger 是日志库的主要接口类，支持多种日志级别（TRACE、DEBUG、INFO、WARN、ERROR、FATAL）。
 * 支持格式化日志输出和多个日志输出目标的添加。
 */
class CLogger {
public:
    /**
     * @brief 移动构造函数（已删除）
     */
    CLogger( CLogger&& oth ) = delete;

    /**
     * @brief 移动赋值运算符（已删除）
     */
    CLogger& operator=( CLogger&& oth ) = delete;

    /**
     * @brief 析构函数
     */
    ~CLogger() = default;

public:
    /**
     * @brief 构造函数
     */
    explicit CLogger() : _impl( std::make_unique< CLoggerImpl >() ) {}

    /**
     * @brief 记录 INFO 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    bool info( std::string_view fmt, Args&&... args ) const {
        return this->_impl->info( fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 TRACE 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    bool trace( std::string_view fmt, Args&&... args ) const {
        return this->_impl->trace( fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 DEBUG 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    bool debug( std::string_view fmt, Args&&... args ) const {
        return this->_impl->debug( fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 WARN 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    bool warn( std::string_view fmt, Args&&... args ) const {
        return this->_impl->warn( fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 ERROR 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    bool error( std::string_view fmt, Args&&... args ) const {
        return this->_impl->error( fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 记录 FATAL 级别日志
     * @tparam Args 可变模板参数类型
     * @param fmt 格式化字符串
     * @param args 格式化参数
     * @return 成功返回 true，失败返回 false
     */
    template < class... Args >
    bool fatal( std::string_view fmt, Args&&... args ) const {
        return this->_impl->fatal( fmt, std::forward< Args >( args )... );
    }

    /**
     * @brief 添加日志输出目标
     * @param sink 日志输出目标的智能指针
     * @return 成功返回 true，失败返回 false
     */
    bool add_sink( std::unique_ptr< sinks::ISink > sink ) const {
        return _impl->add_sink( std::move( sink ) );
    }

private:
    std::unique_ptr< CLoggerImpl > _impl;  // 日志实现类智能指针
};

}  // namespace jzlog
