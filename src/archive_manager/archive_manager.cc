#include "jzlog/archive_manager/archive_manager.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace jzlog
{
namespace sinks
{

/**
 * @brief 构造函数：初始化归档管理器
 * @param config 归档配置参数
 *
 * 初始化步骤：
 * 1. 保存配置参数
 * 2. 根据配置构建各子目录路径
 * 3. 创建必要的目录结构（current/、archived/、tar/、compressed/）
 */
CArchiveManager::CArchiveManager( const ArchiveConfig& config ) noexcept :
    _config( config ),
    _running( false ),
    _worker_thread(),
    _mutex(),
    _cond_var(),
    _current_dir( _config.base_path + "/current" ),
    _archived_dir( _config.base_path + "/archived" ),
    _tar_dir( _config.base_path + "/tar" ),
    _compressed_dir( _config.base_path + "/compressed" ) {
    // 创建所需的目录结构
    try {
        std::filesystem::create_directories( _current_dir );
        std::filesystem::create_directories( _archived_dir );
        std::filesystem::create_directories( _tar_dir );
        std::filesystem::create_directories( _compressed_dir );
    } catch ( const std::exception& e ) {
        // 目录创建失败，记录错误但不抛出异常
        // TODO: 可以考虑添加错误日志记录
    }
}

/**
 * @brief 析构函数：确保后台线程安全停止
 */
CArchiveManager::~CArchiveManager() { stop(); }

/**
 * @brief 启动归档管理器
 * @details 使用原子操作确保只启动一次线程，避免重复启动
 */
void CArchiveManager::start() noexcept {
    // exchange 返回旧值，如果之前是 false，说明需要启动
    if ( !_running.exchange( true ) ) {
        _worker_thread = std::thread( &CArchiveManager::worker_thread, this );
    }
}

/**
 * @brief 停止归档管理器
 * @details 安全地停止后台线程，确保资源正确释放
 */
void CArchiveManager::stop() noexcept {
    // exchange 返回旧值，如果之前是 true，说明需要停止
    if ( _running.exchange( false ) ) {
        // 通知等待线程
        _cond_var.notify_all();
        // 等待线程退出
        if ( _worker_thread.joinable() ) {
            _worker_thread.join();
        }
    }
}

/**
 * @brief 立即执行所有归档任务（用于测试和调试）
 */
void CArchiveManager::trigger_pack_now() noexcept {
    perform_daily_pack();
    check_and_compress();
    cleanup_expired_files();
}

/**
 * @brief 更新归档配置
 * @param config 新的配置参数
 */
void CArchiveManager::update_config( const ArchiveConfig& config ) noexcept {
    std::lock_guard< std::mutex > lock( _mutex );
    _config = config;

    // 更新目录路径
    _current_dir    = _config.base_path + "/current";
    _archived_dir   = _config.base_path + "/archived";
    _tar_dir        = _config.base_path + "/tar";
    _compressed_dir = _config.base_path + "/compressed";

    // 确保新配置的目录存在
    try {
        std::filesystem::create_directories( _current_dir );
        std::filesystem::create_directories( _archived_dir );
        std::filesystem::create_directories( _tar_dir );
        std::filesystem::create_directories( _compressed_dir );
    } catch ( const std::exception& e ) {
        // 目录创建失败
    }
}

/**
 * @brief 后台工作线程的主循环
 * @details 线程循环执行以下任务：
 * 1. 等待到配置的打包时间（默认凌晨 2 点）
 * 2. 执行每日日志打包
 * 3. 检查并执行压缩任务
 * 4. 清理过期的归档文件
 */
void CArchiveManager::worker_thread() noexcept {
    while ( _running ) {
        std::unique_lock< std::mutex > lock( _mutex );

        // 计算下次打包时间
        auto next_pack_time = calculate_next_pack_time();

        // 等待直到到达打包时间或被通知
        if ( _cond_var.wait_until( lock, next_pack_time, [ this ]() {
                 return !_running;
             } ) ) {
            break;  // 被通知停止
        }

        // 执行每日打包
        if ( _config.enable_archive ) {
            lock.unlock();
            perform_daily_pack();
            lock.lock();
        }

        // 检查并压缩
        if ( _config.enable_compress ) {
            lock.unlock();
            check_and_compress();
            lock.lock();
        }

        // 清理过期文件
        if ( _config.enable_cleanup ) {
            lock.unlock();
            cleanup_expired_files();
            lock.lock();
        }
    }
}

/**
 * @brief 计算下次打包的时间点
 * @return 下次打包的时间点（system_clock::time_point）
 * @note 如果当前时间已超过今天的打包时间，则返回明天的打包时间
 */
std::chrono::system_clock::time_point CArchiveManager::calculate_next_pack_time() const noexcept {
    auto     now        = std::chrono::system_clock::now();
    auto     time_t_now = std::chrono::system_clock::to_time_t( now );
    std::tm* local_now  = std::localtime( &time_t_now );

    // 设置目标时间
    std::tm target = *local_now;
    target.tm_hour = static_cast< int >( _config.pack_hour );
    target.tm_min  = static_cast< int >( _config.pack_minute );
    target.tm_sec  = 0;

    // 转换回 time_point
    auto target_time = std::chrono::system_clock::from_time_t( std::mktime( &target ) );

    // 如果目标时间已过，设置为明天
    if ( target_time <= now ) {
        target_time += std::chrono::hours( 24 );
    }

    return target_time;
}

/**
 * @brief 执行每日日志打包任务
 * @details 执行流程：
 * 1. 获取前一天的日期字符串
 * 2. 将 current/ 目录中前一天的日志移动到 archived/YYYYMMDD/
 * 3. 使用 tar 命令将 archived/YYYYMMDD/ 打包成 tar/logs_YYYYMMDD.tar
 */
void CArchiveManager::perform_daily_pack() noexcept {
    // 1. 获取昨天的日期字符串
    auto     yesterday        = std::chrono::system_clock::now() - std::chrono::hours( 24 );
    auto     time_t_yesterday = std::chrono::system_clock::to_time_t( yesterday );
    std::tm* tm_yesterday     = std::localtime( &time_t_yesterday );

    char date_str[ 16 ];
    std::strftime( date_str, sizeof( date_str ), "%Y%m%d", tm_yesterday );

    // 2. 移动 current/ 中的日志文件到 archived/YYYYMMDD/
    std::vector< std::filesystem::path > log_files = get_log_files_by_date( date_str );

    for ( const auto& file : log_files ) {
        move_to_archived( file.string() );
    }

    // 3. 创建 tar 归档
    create_tar_archive( date_str );
}

/**
 * @brief 检查并执行压缩任务
 * @details 当 tar/ 目录中所有文件的总大小超过配置的阈值时，
 *          将所有 tar 文件使用 zstd 压缩，并删除原始 tar 文件和 archived/ 中的原始日志
 */
void CArchiveManager::check_and_compress() noexcept {
    // 1. 计算 tar 目录中所有文件的总大小
    uint64_t total_size = calculate_tar_dir_size();

    // 2. 如果超过阈值，执行压缩
    if ( total_size >= _config.compress_threshold ) {
        // 获取所有 tar 文件
        std::vector< std::filesystem::path > tar_files;
        try {
            if ( std::filesystem::exists( _tar_dir ) ) {
                for ( const auto& entry : std::filesystem::directory_iterator( _tar_dir ) ) {
                    if ( entry.path().extension() == ".tar" ) {
                        tar_files.emplace_back( entry.path() );
                    }
                }
            }
        } catch ( const std::exception& e ) {
            // 遍历失败
        }

        // 压缩每个 tar 文件
        for ( const auto& tar_file : tar_files ) {
            compress_with_zstd( tar_file.string() );
        }
    }
}

/**
 * @brief 清理过期的归档文件
 * @details 遍历 compressed/、tar/、archived/ 目录，删除修改时间
 *          超过配置的保留天数的文件和目录
 */
void CArchiveManager::cleanup_expired_files() noexcept {
    auto now             = std::chrono::system_clock::now();
    auto expiration_time = now - std::chrono::hours( 24 * _config.retention_days );

    try {
        // 1. 清理 compressed/ 中的过期文件
        if ( std::filesystem::exists( _compressed_dir ) ) {
            for ( const auto& entry : std::filesystem::directory_iterator( _compressed_dir ) ) {
                if ( entry.is_regular_file() && is_file_expired( entry.path() ) ) {
                    std::filesystem::remove( entry.path() );
                }
            }
        }

        // 2. 清理 tar/ 中的过期文件
        if ( std::filesystem::exists( _tar_dir ) ) {
            for ( const auto& entry : std::filesystem::directory_iterator( _tar_dir ) ) {
                if ( entry.is_regular_file() && is_file_expired( entry.path() ) ) {
                    std::filesystem::remove( entry.path() );
                }
            }
        }

        // 3. 清理 archived/ 中的过期目录
        if ( std::filesystem::exists( _archived_dir ) ) {
            for ( const auto& entry : std::filesystem::directory_iterator( _archived_dir ) ) {
                if ( entry.is_directory() && is_file_expired( entry.path() ) ) {
                    std::filesystem::remove_all( entry.path() );
                }
            }
        }
    } catch ( const std::exception& e ) {
        // 清理失败
    }
}

/**
 * @brief 执行 tar 打包操作
 * @param date_str 日期字符串（格式：YYYYMMDD）
 * @return 成功返回 true，失败返回 false
 */
bool CArchiveManager::create_tar_archive( const std::string& date_str ) noexcept {
    try {
        std::filesystem::path archived_dir = std::filesystem::path( _archived_dir ) / date_str;
        std::filesystem::path tar_file =
            std::filesystem::path( _tar_dir ) / ( "logs_" + date_str + ".tar" );

        // 检查源目录是否存在且有文件
        if ( !std::filesystem::exists( archived_dir ) ) {
            return false;
        }

        // 构建 tar 命令
        std::stringstream cmd;
        cmd << "tar -cf " << tar_file.string() << " -C " << archived_dir.parent_path().string()
            << " " << date_str << " 2>/dev/null";

        return execute_command( cmd.str() );
    } catch ( const std::exception& e ) {
        return false;
    }
}

/**
 * @brief 使用 zstd 压缩 tar 文件
 * @param tar_path tar 文件的完整路径
 * @return 成功返回 true，失败返回 false
 * @note 如果系统未安装 zstd，会回退到只移动文件不压缩
 */
bool CArchiveManager::compress_with_zstd( const std::string& tar_path ) noexcept {
    try {
        std::filesystem::path src( tar_path );
        std::filesystem::path dest =
            std::filesystem::path( _compressed_dir ) / ( src.stem().string() + ".tar.zst" );

        // 检查 zstd 是否可用
        if ( !check_zstd_available() ) {
            // 回退：只移动 tar 文件，不压缩
            std::filesystem::rename( src, dest );
            return true;
        }

        // 构建 zstd 命令
        std::stringstream cmd;
        cmd << "zstd -q -f " << tar_path << " -o " << dest.string() << " 2>/dev/null";

        bool success = execute_command( cmd.str() );

        if ( success ) {
            // 删除原始 tar 文件
            std::filesystem::remove( src );

            // 删除 archived/ 中的原始日志文件
            std::string           date_str = src.stem().string().substr( 5 );  // "logs_" 后的部分
            std::filesystem::path archived_dir = std::filesystem::path( _archived_dir ) / date_str;

            if ( std::filesystem::exists( archived_dir ) ) {
                std::filesystem::remove_all( archived_dir );
            }
        }

        return success;
    } catch ( const std::exception& e ) {
        return false;
    }
}

/**
 * @brief 将日志文件移动到归档目录
 * @param file_path 日志文件的完整路径
 * @return 成功返回 true，失败返回 false
 * @details 从文件名中提取日期，移动到 archived/YYYYMMDD/ 目录
 */
bool CArchiveManager::move_to_archived( const std::string& file_path ) noexcept {
    try {
        std::filesystem::path src( file_path );

        // 从文件名提取日期（假设格式：app_20250127_143022.log）
        std::string filename = src.filename().string();

        // 创建目标目录：archived/YYYYMMDD/
        size_t pattern_len = _config.log_file_pattern.size();
        if ( filename.size() < pattern_len + 8 ) {
            return false;                                                    // 文件名格式不正确
        }

        std::string date_str           = filename.substr( pattern_len, 8 );  // 提取日期部分

        std::filesystem::path dest_dir = std::filesystem::path( _archived_dir ) / date_str;
        std::filesystem::create_directories( dest_dir );

        // 移动文件
        std::filesystem::path dest = dest_dir / filename;
        std::filesystem::rename( src, dest );

        return true;
    } catch ( const std::exception& e ) {
        return false;
    }
}

/**
 * @brief 计算 tar 目录中所有文件的总大小
 * @return 总大小（字节）
 */
uint64_t CArchiveManager::calculate_tar_dir_size() const noexcept {
    uint64_t total_size = 0;

    try {
        if ( std::filesystem::exists( _tar_dir ) ) {
            for ( const auto& entry : std::filesystem::directory_iterator( _tar_dir ) ) {
                if ( entry.is_regular_file() && entry.path().extension() == ".tar" ) {
                    total_size += entry.file_size();
                }
            }
        }
    } catch ( const std::exception& e ) {
        // 返回 0 表示出错
    }

    return total_size;
}

/**
 * @brief 获取指定日期的所有日志文件
 * @param date_str 日期字符串（格式：YYYYMMDD）
 * @return 匹配的日志文件路径列表
 * @details 从 current/ 目录中查找文件名包含指定日期的 .log 文件
 */
std::vector< std::filesystem::path >
CArchiveManager::get_log_files_by_date( const std::string& date_str ) const noexcept {
    std::vector< std::filesystem::path > result;

    try {
        if ( std::filesystem::exists( _current_dir ) ) {
            for ( const auto& entry : std::filesystem::directory_iterator( _current_dir ) ) {
                if ( entry.is_regular_file() && entry.path().extension() == ".log" ) {
                    std::string filename = entry.path().filename().string();
                    // 检查文件名是否包含日期
                    if ( filename.find( date_str ) != std::string::npos ) {
                        result.emplace_back( entry.path() );
                    }
                }
            }
        }
    } catch ( const std::exception& e ) {
        // 遍历失败
    }

    return result;
}

/**
 * @brief 判断文件是否已过期
 * @param file_path 文件路径
 * @return 过期返回 true，否则返回 false
 * @details 根据文件的修改时间和配置的保留天数判断
 */
bool CArchiveManager::is_file_expired( const std::filesystem::path& file_path ) const noexcept {
    try {
        auto ftime = std::filesystem::last_write_time( file_path );
        auto sctp  = std::chrono::time_point_cast< std::chrono::system_clock::duration >(
            ftime - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now() );

        auto now = std::chrono::system_clock::now();
        auto age = now - sctp;

        return age > std::chrono::hours( 24 * _config.retention_days );
    } catch ( const std::exception& e ) {
        return false;
    }
}

/**
 * @brief 执行 shell 命令
 * @param cmd 要执行的命令字符串
 * @return 成功返回 true，失败返回 false
 * @note 使用 popen 执行命令，返回命令的退出状态
 */
bool CArchiveManager::execute_command( const std::string& cmd ) const noexcept {
    try {
        // 使用 popen 执行命令
        FILE* pipe = popen( cmd.c_str(), "r" );
        if ( !pipe ) {
            return false;
        }

        // 读取输出（用于调试）
        char buffer[ 128 ];
        while ( fgets( buffer, sizeof( buffer ), pipe ) != nullptr ) {
            // 可以选择性地记录输出
        }

        int status = pclose( pipe );
        return ( status == 0 );
    } catch ( const std::exception& e ) {
        return false;
    }
}

/**
 * @brief 检查系统是否安装了 zstd
 * @return 可用返回 true，否则返回 false
 * @note 使用 "which zstd" 命令检测
 */
bool CArchiveManager::check_zstd_available() const noexcept {
    // 使用 system 检查 zstd 是否可用
    return ( system( "which zstd > /dev/null 2>&1" ) == 0 );
}

}  // namespace sinks
}  // namespace jzlog
