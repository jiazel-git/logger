#include "jzlog/archive_manager/archive_manager.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

CArchiveManager::~CArchiveManager() { stop(); }

bool CArchiveManager::start() noexcept {
    // exchange 返回旧值，如果之前是 false，说明需要启动
    if ( !_running.exchange( true ) ) {
        _worker_thread = std::thread( &CArchiveManager::worker_thread, this );
    }
    return true;
}

bool CArchiveManager::stop() noexcept {
    // exchange 返回旧值，如果之前是 true，说明需要停止
    if ( _running.exchange( false ) ) {
        // 通知等待线程
        _cond_var.notify_all();
        // 等待线程退出
        if ( _worker_thread.joinable() ) {
            _worker_thread.join();
        }
    }
    return true;
}

void CArchiveManager::trigger_pack_now() noexcept {
    perform_daily_pack();
    check_and_compress();
    cleanup_expired_files();
}

void CArchiveManager::update_config( const ArchiveConfig& config ) noexcept {
    std::lock_guard lock( _mutex );
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

decltype( auto ) CArchiveManager::calculate_next_pack_time() const noexcept {
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
        target_time += std::chrono::hours( kHoursPerDay );
    }

    return target_time;
}

void CArchiveManager::worker_thread() noexcept {
    while ( _running ) {
        std::unique_lock lock( _mutex );

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

decltype( auto )
CArchiveManager::get_log_files_by_date( const std::string& date_str ) const noexcept {
    std::vector< std::filesystem::path > result;

    try {
        if ( std::filesystem::exists( _current_dir ) ) {
            for ( const auto& entry : std::filesystem::directory_iterator( _current_dir ) ) {
                if ( entry.is_regular_file() ) {
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

void CArchiveManager::perform_daily_pack() noexcept {
    // 1. 获取昨天的日期字符串
    auto     yesterday = std::chrono::system_clock::now() - std::chrono::hours( kHoursPerDay );
    auto     time_t_yesterday = std::chrono::system_clock::to_time_t( yesterday );
    std::tm* tm_yesterday     = std::localtime( &time_t_yesterday );

    char date_str[ kDateBufferSize ];
    std::strftime( date_str, sizeof( date_str ), "%Y%m%d", tm_yesterday );

    // 2. 移动 current/ 中的日志文件到 archived/YYYYMMDD/
    auto log_files = get_log_files_by_date( date_str );

    for ( const auto& file : log_files ) {
        move_to_archived( file.string() );
    }

    // 3. 创建 tar 归档
    create_tar_archive( date_str );
}

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

bool CArchiveManager::create_tar_archive( const std::string& date_str ) noexcept {
    try {
        std::filesystem::path archived_dir = std::filesystem::path( _archived_dir ) / date_str;
        std::filesystem::path tar_file = std::filesystem::path( _tar_dir ) / ( date_str + ".tar" );

        // 检查源目录是否存在且有文件
        if ( !std::filesystem::exists( archived_dir ) ) {
            return false;
        }

        // 构建 tar 命令
        std::stringstream cmd;
        cmd << "tar -cf " << tar_file.string() << " -C " << archived_dir.parent_path().string()
            << " " << date_str << " 2>/dev/null";

        bool success = execute_command( cmd.str() );

        if ( success ) {
            std::filesystem::remove_all( archived_dir );
        }

        return success;
    } catch ( const std::exception& e ) {
        return false;
    }
}

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
            std::string           date_str     = src.stem().string();
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

bool CArchiveManager::move_to_archived( const std::string& file_path ) noexcept {
    try {
        std::filesystem::path src( file_path );

        std::string filename           = src.filename().string();

        std::string date_str           = filename.substr( 0, kDateStringLength );

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

bool CArchiveManager::check_zstd_available() const noexcept {
    // 使用 system 检查 zstd 是否可用
    return ( system( "which zstd > /dev/null 2>&1" ) == 0 );
}

}  // namespace sinks
}  // namespace jzlog
