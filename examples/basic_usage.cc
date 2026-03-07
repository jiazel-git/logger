#include "jzlog/logger.hpp"
#include "jzlog/sinks/file_sink.h"
#include <chrono>
#include <memory>
#include <thread>

/// 这是一个基本的使用示例
/// 演示如何使用 jzlog 日志库
int main() {
    // 创建日志器
    jzlog::CLogger logger;

    // 添加文件输出 sink
    logger.add_sink( std::make_unique< jzlog::sinks::CFileSink >() );

    // 记录不同级别的日志
    logger.info( "应用程序启动" );
    logger.debug( "调试信息: 当前配置已加载" );
    logger.warn( "警告: 使用默认配置" );

    // 支持格式化输出
    for ( int i = 0; i < 5; ++i ) {
        logger.info( "处理任务 #%d - 进度: %d%%", i + 1, ( i + 1 ) * 20 );
        std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    }

    logger.info( "应用程序结束" );

    return 0;
}
