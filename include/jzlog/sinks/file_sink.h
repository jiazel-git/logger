/**
 * @file file_sink.h
 * @brief 文件日志 Sink 实现类
 * @details 提供基于文件的异步日志输出功能，支持：
 *          - 双缓冲区异步写入
 *          - 按大小自动文件轮转
 *          - 按日期自动创建新文件
 *          - 可选的自动归档功能
 * @author carbon
 * @date 2026-03-29
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
/**
 * @brief 默认单个日志文件大小（100MB）
 */
constexpr int DEFAULT_FILE_SIZE{ 100 * 1024 * 1024 };
/**
 * @brief 默认日志文件路径
 */
constexpr std::string_view DEFAULT_FILE_PATH{ "/home/carbon/workspace/logger/log" };
/**
 * @class CFileSink
 * @brief 文件日志 Sink 实现类
 * @details 该类实现了异步文件日志输出，具有以下特性：
 *          - **双缓冲区设计**：使用当前缓冲区和备用缓冲区，后台线程异步写入磁盘
 *          - **自动文件轮转**：当文件大小超过阈值时自动创建新文件
 *          - **按日期分组**：文件名格式为 YYYYMMDD_NNN
 *          - **线程安全**：使用互斥锁和条件变量保证并发安全
 *          - **RAII 资源管理**：析构造时自动停止工作线程并刷新缓冲区
 *
 * @note 该类不可拷贝和移动，所有权必须通过智能指针管理
 */
class CFileSink final : public ISink {
public:
    /**
     * @brief 缓冲区类型定义（4MB 固定缓冲区）
     */
    using Buffer = utils::FixedBuffer< utils::kLargeBuffer >;
    /**
     * @brief 缓冲区智能指针类型
     */
    using BufferPtr = std::unique_ptr< Buffer >;
    /**
     * @brief 缓冲区向量类型
     */
    using BufferVec = std::vector< BufferPtr >;

public:
    /**
     * @brief 默认构造函数
     * @details 使用默认配置创建 FileSink：
     *          - 日志级别：TRACE
     *          - 文件大小：100MB
     *          - 文件路径：默认路径
     *          - 自动启动后台写入线程
     * @note 使用 noexcept 标记，但内部操作可能抛出异常
     */
    explicit CFileSink() noexcept;
    /**
     * @brief 参数化构造函数
     * @param level 日志过滤级别，等于或高于此级别的日志会被记录
     * @param fileSize 单个日志文件的最大大小（字节）
     * @param bufSize 缓冲区大小（当前未使用，保留接口）
     * @param dir 日志文件存储目录路径
     * @param enable 是否启用 Sink（当前未实现）
     * @note 使用 noexcept 标记，但内部操作可能抛出异常
     */
    explicit CFileSink( LogLevel level, uint32_t fileSize, uint32_t bufSize, std::string dir,
                        bool enable ) noexcept;
    /**
     * @brief 带归档配置的构造函数
     * @param level 日志过滤级别
     * @param fileSize 单个日志文件的最大大小（字节）
     * @param bufSize 缓冲区大小
     * @param enable 是否启用 Sink
     * @param archiveConfig 归档配置（当前未实现）
     * @note 使用 noexcept 标记，但内部操作可能抛出异常
     */
    explicit CFileSink( LogLevel level, uint32_t fileSize, uint32_t bufSize, bool enable,
                        const ArchiveConfig& archiveConfig ) noexcept;
    /**
     * @brief 禁用拷贝构造函数
     */
    CFileSink( const CFileSink& ) = delete;
    /**
     * @brief 禁用拷贝赋值运算符
     */
    CFileSink& operator=( const CFileSink& ) = delete;
    /**
     * @brief 禁用移动构造函数
     */
    CFileSink( CFileSink&& ) = delete;
    /**
     * @brief 禁用移动赋值运算符
     */
    CFileSink& operator=( CFileSink&& ) = delete;
    /**
     * @brief 写入日志记录
     * @param r 日志记录结构体
     * @details 将日志记录格式化后写入当前缓冲区。
     *          如果当前缓冲区已满，则将其移入待写入队列并切换到备用缓冲区。
     *          该方法由用户线程调用，是线程安全的。
     */
    void write( const LogRecord& r ) override;
    /**
     * @brief 刷新缓冲区到文件
     * @details 将当前缓冲区和所有待写入缓冲区立即刷新到磁盘。
     *          该方法可用于确保关键日志在程序退出前被持久化。
     * @note 该操作会阻塞直到所有缓冲区数据写入完成
     */
    void flush() override;
    /**
     * @brief 设置最低日志级别
     * @param lvl 等于或高于此级别的日志会被记录
     * @details 使用原子操作设置，线程安全
     */
    void set_level( LogLevel lvl ) noexcept override;
    /**
     * @brief 获取当前日志级别
     * @return 当前设置的最低日志级别
     */
    LogLevel level() const noexcept override;
    /**
     * @brief 检查是否应该记录指定级别的日志
     * @param lvl 要检查的日志级别
     * @return true 如果该级别应该被记录，false 否则
     */
    bool should_log( LogLevel lvl ) const noexcept override;
    /**
     * @brief 启用或禁用 Sink
     * @param enable true 启用，false 禁用
     * @details 使用原子操作设置，线程安全。
     *          禁用后，所有写入操作将立即返回，不记录日志。
     */
    void set_enabled( bool enable ) noexcept override;
    /**
     * @brief 检查 Sink 是否启用
     * @return true 如果 Sink 启用，false 否则
     */
    bool enabled() const noexcept override;
    /**
     * @brief 析构函数
     * @details 执行以下清理操作：
     *          1. 停止后台工作线程
     *          2. 刷新所有缓冲区到磁盘
     *          3. 关闭文件流
     * @note 确保所有日志数据在对象销毁前被持久化
     */
    ~CFileSink();
    /**
     * @brief 启用或禁用自动归档
     * @param enable true 启用归档，false 禁用
     * @details 当前未实现，保留接口供未来扩展
     *          归档功能包括：
     *          - 定时打包历史日志文件
     *          - 压缩归档文件
     *          - 清理过期归档
     */
    void enable_archive( bool enable ) noexcept;

private:
    /**
     * @brief 创建新的日志文件
     * @details 根据当前日期和文件索引生成文件名（格式：YYYYMMDD_NNN）。
     *          如果日期变化，则重置索引为 0；否则递增索引。
     *          自动创建必要的目录结构。
     * @note 文件以追加模式打开（std::ios::app）
     */
    void create_new_file() noexcept;
    /**
     * @brief 格式化日志记录
     * @param r 日志记录结构体
     * @return 格式化后的日志字符串
     * @details 格式：[时间] [级别] [线程ID] [文件:行号] 消息\n
     *          示例：2026-03-29 10:30:15 [INFO] [140123456789] [main:42] Hello World\n
     */
    std::string format_log_record( const LogRecord& r );
    /**
     * @brief 将缓冲区数据写入文件
     * @param buffer 要写入的缓冲区智能指针
     * @details 检查文件大小是否超过阈值，如果超过则触发轮转。
     *          使用文件锁保证线程安全。
     * @note 调用后 buffer 的所有权被转移，变为空指针
     */
    void flush_buffer_to_file( BufferPtr buffer ) noexcept;
    /**
     * @brief 执行文件轮转（公共接口）
     * @details 当前未使用，保留供内部调用
     */
    void rotate_file();
    /**
     * @brief 执行文件轮转（私有实现）
     * @details 1. 刷新并关闭当前文件流
     *          2. 创建新的日志文件（create_new_file）
     * @note 该方法必须在持有 _file_mutex 锁的情况下调用
     */
    void rotate_file_();
    /**
     * @brief 处理过期文件
     * @details 当前未实现，保留接口供归档功能使用
     */
    void process_expired_file();
    /**
     * @brief 启动后台工作线程
     * @details 如果线程未运行，则创建并启动新线程执行 work_thread
     * @note 使用 exchange 原子操作，避免重复启动
     */
    void start() noexcept;
    /**
     * @brief 后台工作线程主循环
     * @details 线程执行逻辑：
     *          1. 等待条件变量（最多 3 秒超时）或被通知
     *          2. 如果当前缓冲区有数据，则将其移入待写入队列并切换缓冲区
     *          3. 将所有待写入缓冲区写入文件
     *          4. 循环直到 _running 标为 false
     *          5. 退出前刷新所有剩余缓冲区
     * @note 线程安全：使用互斥锁保护缓冲区操作
     */
    void work_thread() noexcept;
    /**
     * @brief 初始化文件索引
     * @details 扫描日志目录，找到当前日期的最大文件索引，
     *          设置 _cur_idx 为最大索引 + 1 或 0。
     *          文件名格式：YYYYMMDD_NNN
     * @note 启动时调用，确保从正确索引继续记录
     */
    void init_file_idx() noexcept;
    /**
     * @brief 获取当前日期字符串
     * @return 格式化的日期字符串（YYYYMMDD）
     * @details 使用线程安全的 localtime 替代 std::localtime
     */
    std::string get_date_str() noexcept;

private:
    /**
     * @brief 日志过滤级别
     * @details 等于或高于此级别的日志会被记录
     */
    LogLevel _level;
    /**
     * @brief 单个日志文件的最大大小（字节）
     * @details 默认值：100MB
     */
    uint32_t _file_size;
    /**
     * @brief 日志文件存储目录路径
     * @details 默认值：/home/carbon/workspace/logger/log
     */
    std::string _file_path;
    /**
     * @brief 当前日志文件名（不含路径）
     * @details 格式：YYYYMMDD_NNN
     */
    std::string _cur_file_name;
    /**
     * @brief 当前日志文件的已写入大小（字节）
     * @details 用于检测是否需要文件轮转
     */
    uint32_t _cur_file_size;
    /**
     * @brief 当前写入缓冲区
     * @details 用户线程向此缓冲区写入日志，由工作线程读取
     */
    BufferPtr _current_buffer;
    /**
     * @brief 备用写缓冲区
     * @details 当当前缓冲区满时，切换到此缓冲区
     */
    BufferPtr _next_buffer;
    /**
     * @brief 待写入的缓冲区队列
     * @details 工作线程从此队列取缓冲区并写入文件
     */
    BufferVec _buffers;
    /**
     * @brief 写缓冲区互斥锁
     * @details 保护 _current_buffer, _next_buffer, _buffers 的并发访问
     */
    std::mutex _buffer_mutex;
    /**
     * @brief 条件变量
     * @details 用于在工作线程中等待新数据或停止信号
     */
    std::condition_variable _cond;
    /**
     * @brief 文件输出流
     * @details 当前打开的日志文件流
     */
    std::ofstream _file_stream;
    /**
     * @brief 当前文件索引
     * @details 用于生成文件名：YYYYMMDD_NNN
     */
    int _cur_idx;
    /**
     * @brief 当前日期字符串（YYYYMMDD）
     * @details 用于检测日期变化并重置文件索引
     */
    std::string _cur_date_str;
    /**
     * @brief 后台工作线程
     * @details 执行异步文件写入操作
     */
    std::thread _thread;
    /**
     * @brief 线程运行标志
     * @details true 表示工作线程正在运行，false 表示应该停止
     * @note 使用 atomic 保证线程安全
     */
    std::atomic< bool > _running;
    /**
     * @brief Sink 启用状态
     * @details true 表示 Sink 启用，false 表示禁用
     * @note 使用 atomic 保证线程安全，默认为 true
     */
    std::atomic< bool > _enabled{ true };
    /**
     * @brief 文件操作互斥锁
     * @details 保护 _file_stream 和文件轮转操作的并发访问
     */
    std::mutex _file_mutex;
};
}  // namespace sinks
}  // namespace jzlog
