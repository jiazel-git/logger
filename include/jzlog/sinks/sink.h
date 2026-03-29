#pragma once

#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"

namespace jzlog
{
namespace sinks
{

/**
 * @brief Sink 接口
 * @details 定义日志输出目标的抽象基类
 */
class ISink {
public:
    virtual ~ISink() = default;

    // ==================== 核心功能 ====================

    /**
     * @brief 写入日志记录
     * @param record 日志记录
     * @note 这是 Sink 的核心功能
     */
    virtual void write( const LogRecord& record ) = 0;

    /**
     * @brief 刷新缓冲区
     * @note 确保所有缓冲数据写入目标
     */
    virtual void flush() = 0;

    // ==================== 级别过滤 ====================

    /**
     * @brief 设置最低日志级别
     * @param level 等于或高于此级别的日志会被记录
     */
    virtual void set_level( LogLevel level ) noexcept = 0;

    /**
     * @brief 获取当前日志级别
     */
    virtual LogLevel level() const noexcept = 0;

    /**
     * @brief 检查是否应该记录指定级别的日志
     * @param level 要检查的日志级别
     */
    virtual bool should_log( LogLevel level ) const noexcept = 0;

    // ==================== 开关控制 ====================

    /**
     * @brief 启用或禁用 Sink
     * @param enabled true 启用，false 禁用
     */
    virtual void set_enabled( bool enabled ) noexcept = 0;

    /**
     * @brief 检查 Sink 是否启用
     */
    virtual bool enabled() const noexcept = 0;
};

}  // namespace sinks
}  // namespace jzlog
