/**
 * @file network_sink.h
 * @brief 网络日志 Sink 实现类（TCP + 自动重连 + 批量发送 + 压缩传输）
 */
#pragma once

#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/sinks/sink.h"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace jzlog
{
namespace sinks
{

constexpr int              DEFAULT_BATCH_SIZE{ 100 };
constexpr uint32_t         DEFAULT_BATCH_TIMEOUT_MS{ 1000 };
constexpr uint32_t         DEFAULT_RETRY_INTERVAL_MS{ 5000 };
constexpr uint32_t         MAX_RETRY_INTERVAL_MS{ 60000 };
constexpr uint32_t         SOCKET_SEND_TIMEOUT_MS{ 5000 };
constexpr int              DEFAULT_SOCKET_BUFFER_SIZE{ 64 * 1024 };
constexpr std::string_view DEFAULT_HOST{ "127.0.0.1" };
constexpr uint16_t         DEFAULT_PORT{ 9999 };

/**
 * @class CNetworkSink
 * @brief 网络日志 Sink 实现类，支持 TCP 连接、自动重连、批量发送和压缩传输
 */
class CNetworkSink final : public ISink {
public:
    /**
     * @brief 默认构造函数
     */
    explicit CNetworkSink() noexcept;

    /**
     * @brief 构造函数
     * @param host 服务器地址
     * @param port 服务器端口
     * @param level 日志级别
     * @param batch_size 批量发送数量
     * @param batch_timeout_ms 批量超时时间（毫秒）
     * @param retry_interval_ms 重连间隔（毫秒）
     * @param enable 是否启用
     */
    explicit CNetworkSink( std::string host, uint16_t port, LogLevel level, size_t batch_size,
                           uint32_t batch_timeout_ms, uint32_t retry_interval_ms,
                           bool enable ) noexcept;

    /**
     * @brief 拷贝构造函数（已删除）
     */
    CNetworkSink( const CNetworkSink& ) = delete;

    /**
     * @brief 拷贝赋值运算符（已删除）
     */
    CNetworkSink& operator=( const CNetworkSink& ) = delete;

    /**
     * @brief 移动构造函数（已删除）
     */
    CNetworkSink( CNetworkSink&& ) = delete;

    /**
     * @brief 移动赋值运算符（已删除）
     */
    CNetworkSink& operator=( CNetworkSink&& ) = delete;

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
     * @param enabled 启用状态
     */
    void set_enabled( bool enabled ) noexcept override;

    /**
     * @brief 获取启用状态
     * @return 启用返回 true，否则返回 false
     */
    bool enabled() const noexcept override;

    /**
     * @brief 析构函数
     */
    ~CNetworkSink();

private:
    /**
     * @brief 建立 TCP 连接
     * @return 成功返回 true，失败返回 false
     */
    bool connect() noexcept;

    /**
     * @brief 断开连接
     */
    void disconnect() noexcept;

    /**
     * @brief 自动重连（指数退避）
     */
    void auto_reconnect() noexcept;

    /**
     * @brief 批量发送日志
     * @return 成功返回 true，失败返回 false
     */
    bool send_batch() noexcept;

    /**
     * @brief 后台工作线程主函数
     */
    void work_thread() noexcept;

    /**
     *brief 格式化日志记录
     * @param r 日志记录
     * @return 格式化后的字符串
     */
    std::string format_log_record( const LogRecord& r );

    /**
     * @brief 设置 socket 超时
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回 true，失败返回 false
     */
    bool set_socket_timeout( uint32_t timeout_ms ) noexcept;

    /**
     * @brief 启动后台工作线程
     */
    void start() noexcept;

private:
    LogLevel            _level;                  // 日志过滤级别
    std::string         _host;                   // 服务器地址
    uint16_t            _port;                   // 服务器端口
    int                 _socket_fd;              // Socket 文件描述符
    std::atomic< bool > _connected;              // 连接状态

    size_t                   _batch_size;        // 批量大小
    uint32_t                 _batch_timeout_ms;  // 批量超时（毫秒）
    std::vector< LogRecord > _batch_buffer;      // 批量缓冲区
    std::mutex               _buffer_mutex;      // 缓冲区互斥锁

    uint32_t _retry_interval_ms;                 // 重连间隔（毫秒）
    uint32_t _retry_backoff;                     // 退避倍数

    std::thread             _thread;             // 后台工作线程
    std::atomic< bool >     _running;            // 线程运行标志
    std::condition_variable _cond;               // 条件变量
    std::atomic< bool >     _enabled;            // 启用状态
};

}  // namespace sinks
}  // namespace jzlog
