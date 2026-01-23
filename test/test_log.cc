#include "jzlog/logger.hpp"
#include "jzlog/slinks/file_sink.h"
#include <chrono>
#include <memory>
#include <thread>

int main( int argc, char* argv[] ) {
    jzlog::CLogger logger;
    logger.add_sink( std::make_unique< jzlog::sinks::CFileSink >() );
    while ( 1 ) {
        logger.info( "test log %s\n", "hello world" );
        std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    }
    return 0;
}
