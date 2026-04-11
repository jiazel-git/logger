#include "jzlog/core/log_level.h"
#include "jzlog/logger.hpp"
#include "jzlog/sinks/network_sink.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

int main() {
    jzlog::CLogger logger;

    auto network_sink =
        std::make_unique< jzlog::sinks::CNetworkSink >( "127.0.0.1",            // 服务器地址
                                                        9999,                   // 端口
                                                        jzlog::LogLevel::INFO,  // 日志级别
                                                        10,                     // 批量 10 条
                                                        1000,                   // 超时 1 秒
                                                        5000,                   // 重连间隔 5 秒
                                                        true                    // 启用
        );

    logger.add_sink( std::move( network_sink ) );

    std::cout << "Starting network sink test..." << std::endl;
    std::cout << "Note: Make sure a TCP server is listening on 127.0.0.1:9999" << std::endl;

    for ( int i = 0; i < 20; ++i ) {
        logger.info( "Network sink test message %d", i );
        logger.debug( "This debug message should not appear (level filter)" );
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }

    std::cout << "Logging complete. Waiting for batch to send..." << std::endl;
    std::this_thread::sleep_for( std::chrono::seconds( 2 ) );

    return 0;
}
