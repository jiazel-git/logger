#include "jzlog/archive_manager/archive_manager.h"
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/sinks/file_sink.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace jzlog;
using namespace jzlog::sinks;

int main() {

    // 创建归档配置
    ArchiveConfig config;
    config.base_path = "../log";

    // 创建带归档功能的 FileSink
    CFileSink sink( LogLevel::INFO, 1024 * 1024, 4096, true, config );

    std::cout << "=== Archive Manager Integration Test ===" << std::endl;
    std::cout << "Log path: " << config.base_path << std::endl;
    std::cout << "Current log files should be in: " << config.base_path << "/current/" << std::endl;
    std::cout << "Archived files should be in: " << config.base_path << "/archived/" << std::endl;
    std::cout << "Tar files should be in: " << config.base_path << "/tar/" << std::endl;
    std::cout << std::endl;

    // 写入一些测试日志
    LogRecord record;
    record._level     = LogLevel::INFO;
    record._function  = "main";
    record._line      = 40;
    record._timestamp = std::chrono::system_clock::now();
    record._thread_id = std::this_thread::get_id();

    // 创建昨天的日期字符串
    auto     yesterday        = std::chrono::system_clock::now() - std::chrono::hours( 24 );
    auto     time_t_yesterday = std::chrono::system_clock::to_time_t( yesterday );
    std::tm* tm_yesterday     = std::localtime( &time_t_yesterday );

    char yesterday_str[ 16 ];
    std::strftime( yesterday_str, sizeof( yesterday_str ), "%Y%m%d", tm_yesterday );
    std::cout << "Target archive date: " << yesterday_str << std::endl;

    // 手动创建昨天的日志文件用于测试归档
    std::string current_dir = config.base_path + "/current";
    std::filesystem::create_directories( current_dir );

    std::string   test_log_file = current_dir + "/" + std::string( yesterday_str ) + "_000.log";
    std::ofstream test_file( test_log_file );
    if ( test_file.is_open() ) {
        for ( int i = 0; i < 50; ++i ) {
            test_file << "Test log line " << i << " from yesterday" << std::endl;
        }
        test_file.close();
        std::cout << "Created test log file: " << test_log_file << std::endl;
    }

    // 写入今天的日志（不会被归档）
    for ( int i = 0; i < 10; ++i ) {
        record._message = "Today's test log message " + std::to_string( i );
        sink.write( record );
    }

    std::cout << "Written 10 log messages for today" << std::endl;
    sink.flush();

    // 等待一下确保写入完成
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

    // 列出 current 目录中的文件
    std::cout << "\n=== Current directory contents ===" << std::endl;
    if ( std::filesystem::exists( current_dir ) ) {
        for ( const auto& entry : std::filesystem::directory_iterator( current_dir ) ) {
            std::cout << "  " << entry.path().filename() << std::endl;
        }
    } else {
        std::cout << "  Current directory not found" << std::endl;
    }

    // 手动触发归档
    std::cout << "\n=== Triggering archive ===" << std::endl;

    CArchiveManager test_archive( config );
    test_archive.start();

    std::cout << "Waiting 2 seconds for archive to complete..." << std::endl;
    std::this_thread::sleep_for( std::chrono::seconds( 2 ) );

    // 手动触发打包
    test_archive.trigger_pack_now();
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

    // 列出 archived 目录中的文件
    std::cout << "\n=== Archived directory contents ===" << std::endl;
    std::string archived_dir = config.base_path + "/archived";
    if ( std::filesystem::exists( archived_dir ) ) {
        for ( const auto& entry : std::filesystem::directory_iterator( archived_dir ) ) {
            std::cout << "  Directory: " << entry.path().filename() << std::endl;
            if ( entry.is_directory() ) {
                for ( const auto& file : std::filesystem::directory_iterator( entry.path() ) ) {
                    std::cout << "    " << file.path().filename() << std::endl;
                }
            }
        }
    } else {
        std::cout << "  Archived directory not found" << std::endl;
    }

    // 列出 tar 目录中的文件
    std::cout << "\n=== Tar directory contents ===" << std::endl;
    std::string tar_dir = config.base_path + "/tar";
    if ( std::filesystem::exists( tar_dir ) ) {
        for ( const auto& entry : std::filesystem::directory_iterator( tar_dir ) ) {
            std::cout << "  " << entry.path().filename() << std::endl;
        }
    } else {
        std::cout << "  Tar directory not found" << std::endl;
    }

    test_archive.stop();

    std::cout << "\n=== Test completed ===" << std::endl;
    std::cout << "Check the directories above to verify the archive functionality." << std::endl;
    std::cout << "To clean up test files, run: rm -rf " << config.base_path << std::endl;

    return 0;
}
