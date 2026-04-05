#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace jzlog
{
namespace sinks
{

inline constexpr uint32_t kDefaultPackHour          = 2;
inline constexpr uint32_t kDefaultPackMinute        = 0;
inline constexpr uint64_t kDefaultCompressThreshold = 100 * 1024 * 1024;
inline constexpr uint32_t kDefaultRetentionDays     = 30;
inline constexpr size_t   kDateStringLength         = 8;
inline constexpr uint32_t kHoursPerDay              = 24;
inline constexpr size_t   kDateBufferSize           = 16;
inline const char*        kDefaultBasePath          = "../log/";

/**
 * @brief 日志归档配置结构体
 * @details 用于配置日志归档、压缩和清理的相关参数
 */
struct ArchiveConfig {
    std::string base_path;         ///< 日志根目录，默认 "../log/"
    uint32_t    pack_hour;         ///< 每日打包时间（小时），默认 2（凌晨2点执行）
    uint32_t    pack_minute;       ///< 每日打包时间（分钟），默认 0
    uint64_t
        compress_threshold;   ///< 压缩阈值（字节），当 tar 文件总大小超过此值时触发压缩，默认 100MB
    uint32_t retention_days;  ///< 日志文件保留天数，超过此天数的文件将被自动删除，默认 30 天
    bool     enable_archive;  ///< 是否启用日志归档功能，默认 true
    bool     enable_compress;  ///< 是否启用 zstd 压缩功能，默认 true
    bool     enable_cleanup;   ///< 是否启用过期文件清理功能，默认 true

    /**
     * @brief 默认构造函数，初始化为默认配置
     */
    ArchiveConfig() :
        base_path( kDefaultBasePath ),
        pack_hour( kDefaultPackHour ),
        pack_minute( kDefaultPackMinute ),
        compress_threshold( kDefaultCompressThreshold ),
        retention_days( kDefaultRetentionDays ),
        enable_archive( true ),
        enable_compress( true ),
        enable_cleanup( true ) {}
};

/**
 * @brief 日志归档管理器
 * @details 负责日志文件的自动打包、压缩和清理管理
 *
 * 功能说明：
 * 1. 每日打包：每天在指定时间（默认凌晨 2 点）将前一天的日志文件打包成 tar 文件
 * 2. 二级压缩：当 tar 文件总大小超过阈值（默认 100MB）时，使用 zstd 进行压缩
 * 3. 过期清理：自动删除超过保留天数（默认 30 天）的归档文件
 *
 * 目录结构：
 * - base_path/current/     - 当前活跃的日志文件
 * - base_path/archived/    - 按日期归档的原始日志（YYYYMMDD/）
 * - base_path/tar/         - tar 打包文件
 * - base_path/compressed/  - zstd 压缩后的文件
 *
 * 线程安全：此类内部使用互斥锁保护共享状态，可安全地在多线程环境中使用
 */
class CArchiveManager {
public:
    /**
     * @brief 构造函数
     * @param config 归档配置参数
     * @note 构造函数会自动创建所需的目录结构（current/、archived/、tar/、compressed/）
     */
    explicit CArchiveManager( const ArchiveConfig& config ) noexcept;

    /**
     * @brief 析构函数
     * @note 会自动停止归档管理器的后台线程
     */
    ~CArchiveManager();

    // 禁止拷贝和移动，确保唯一性和资源安全
    CArchiveManager( const CArchiveManager& )            = delete;
    CArchiveManager& operator=( const CArchiveManager& ) = delete;
    CArchiveManager( CArchiveManager&& )                 = delete;
    CArchiveManager& operator=( CArchiveManager&& )      = delete;

    /**
     * @brief 启动归档管理器
     * @details 启动后台工作线程，开始执行定时归档任务
     * @note 重复调用此函数不会产生副作用
     * @return 是否启动成功
     */
    bool start() noexcept;

    /**
     * @brief 停止归档管理器
     * @details 停止后台工作线程，等待线程安全退出
     * @note 重复调用此函数不会产生副作用
     * @return 是否停止成功
     */
    bool stop() noexcept;

    /**
     * @brief 立即执行所有归档任务（用于测试）
     * @details 手动触发打包、压缩和清理任务，不等待定时器
     * @note 此方法主要用于测试和调试目的
     */
    void trigger_pack_now() noexcept;

    /**
     * @brief 获取当前归档配置
     * @return 当前配置的常量引用
     */
    decltype( auto ) get_config() const noexcept { return _config; }

    /**
     * @brief 更新归档配置
     * @param config 新的配置参数
     * @note 配置更新后会立即生效，并重新创建必要的目录结构
     */
    void update_config( const ArchiveConfig& config ) noexcept;

private:
    /**
     * @brief 后台工作线程的主函数
     * @details 线程循环执行以下任务：
     * 1. 等待到配置的打包时间（默认凌晨 2 点）
     * 2. 执行每日日志打包
     * 3. 检查并执行压缩任务
     * 4. 清理过期的归档文件
     */
    void worker_thread() noexcept;

    /**
     * @brief 计算下次打包的时间点
     * @return 下次打包的时间点（system_clock::time_point）
     * @note 如果当前时间已超过今天的打包时间，则返回明天的打包时间
     */
    decltype( auto ) calculate_next_pack_time() const noexcept;

    /**
     * @brief 执行每日日志打包任务
     * @details 执行流程：
     * 1. 获取前一天的日期字符串
     * 2. 将 current/ 目录中前一天的日志移动到 archived/YYYYMMDD/
     * 3. 使用 tar 命令将 archived/YYYYMMDD/ 打包成 tar/logs_YYYYMMDD.tar
     */
    void perform_daily_pack() noexcept;

    /**
     * @brief 检查并执行压缩任务
     * @details 当 tar/ 目录中所有文件的总大小超过配置的阈值时，
     *          将所有 tar 文件使用 zstd 压缩，并删除原始 tar 文件和 archived/ 中的原始日志
     */
    void check_and_compress() noexcept;

    /**
     * @brief 清理过期的归档文件
     * @details 遍历 compressed/、tar/、archived/ 目录，删除修改时间
     *          超过配置的保留天数的文件和目录
     */
    void cleanup_expired_files() noexcept;

    /**
     * @brief 执行 tar 打包操作
     * @param date_str 日期字符串（格式：YYYYMMDD）
     * @return 成功返回 true，失败返回 false
     */
    bool create_tar_archive( const std::string& date_str ) noexcept;

    /**
     * @brief 使用 zstd 压缩 tar 文件
     * @param tar_path tar 文件的完整路径
     * @return 成功返回 true，失败返回 false
     * @note 如果系统未安装 zstd，会回退到只移动文件不压缩
     */
    bool compress_with_zstd( const std::string& tar_path ) noexcept;

    /**
     * @brief 将日志文件移动到归档目录
     * @param file_path 日志文件的完整路径
     * @return 成功返回 true，失败返回 false
     * @details 从文件名中提取日期，移动到 archived/YYYYMMDD/ 目录
     */
    bool move_to_archived( const std::string& file_path ) noexcept;

    /**
     * @brief 计算 tar 目录中所有文件的总大小
     * @return 总大小（字节）
     */
    uint64_t calculate_tar_dir_size() const noexcept;

    /**
     * @brief 获取指定日期的所有日志文件
     * @param date_str 日期字符串（格式：YYYYMMDD）
     * @return 匹配的日志文件路径列表
     * @details 从 current/ 目录中查找文件名包含指定日期的 .log 文件
     */
    decltype( auto ) get_log_files_by_date( const std::string& date_str ) const noexcept;

    /**
     * @brief 判断文件是否已过期
     * @param file_path 文件路径
     * @return 过期返回 true，否则返回 false
     * @details 根据文件的修改时间和配置的保留天数判断
     */
    bool is_file_expired( const std::filesystem::path& file_path ) const noexcept;

    /**
     * @brief 执行 shell 命令
     * @param cmd 要执行的命令字符串
     * @return 成功返回 true，失败返回 false
     * @note 使用 popen 执行命令，返回命令的退出状态
     */
    bool execute_command( const std::string& cmd ) const noexcept;

    /**
     * @brief 检查系统是否安装了 zstd
     * @return 可用返回 true，否则返回 false
     * @note 使用 "which zstd" 命令检测
     */
    bool check_zstd_available() const noexcept;

private:
    ArchiveConfig           _config;         ///< 归档配置
    std::atomic< bool >     _running;        ///< 后台线程运行标志
    std::thread             _worker_thread;  ///< 后台工作线程
    mutable std::mutex      _mutex;          ///< 互斥锁，保护共享状态
    std::condition_variable _cond_var;       ///< 条件变量，用于定时等待和通知

    // 目录路径缓存
    std::string _current_dir;     ///< 当前日志目录 (base_path/current/)
    std::string _archived_dir;    ///< 归档日志目录 (base_path/archived/)
    std::string _tar_dir;         ///< tar 文件目录 (base_path/tar/)
    std::string _compressed_dir;  ///< 压缩文件目录 (base_path/compressed/)
};

}  // namespace sinks
}  // namespace jzlog
