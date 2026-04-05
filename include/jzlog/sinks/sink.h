/**
 * @file sink.h
 * @brief 日志 sink 接口
 */
#pragma once

#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"

namespace jzlog
{
namespace sinks
{

/**
 * @class ISink
 * @brief 日志 sink 接口类，定义日志输出的标准接口
 */
class ISink {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~ISink() = default;

    /**
     * @brief 写入日志记录
     * @param record 日志记录
     * @return 成功返回 true，失败返回 false
     */
    virtual bool write( const LogRecord& record ) = 0;

    /**
     * @brief 刷新倒缓冲区
     * @return 成功返回 true，失败返回 false
     */
    virtual bool flush() noexcept = 0;

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    virtual void set_level( LogLevel level ) noexcept = 0;

    /**
     * @brief 获取日志级别
     * @return 当前日志级别
     */
    virtual LogLevel level() const noexcept = 0;

    /**
     * @brief 判断是否应该记录该级别的日志
     * @param level 日志级别
     * @return 应该记录返回 true，否则返回 false
     */
    virtual bool should_log( LogLevel level ) const noexcept = 0;

    /**
     * @brief 设置是否启用
     * @param enabled 启用状态
     */
    virtual void set_enabled( bool enabled ) noexcept = 0;

    /**
     * @brief 获取启用状态
     * @return 启用返回 true，否则返回 false
     */
    virtual bool enabled() const noexcept = 0;
};

}  // namespace sinks
}  // namespace jzlog
