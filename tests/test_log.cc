#include "jzlog/logger.hpp"
#include "jzlog/sinks/file_sink.h"
#include <memory>
#include <thread>
#include <vector>

int main( int argc, char* argv[] ) {
    jzlog::CLogger logger;
    logger.add_sink( std::make_unique< jzlog::sinks::CFileSink >() );
    std::vector< std::thread > threads;
    threads.emplace_back( [ & ]() {
        while ( 1 ) {
            logger.info( "test log %s\n", "hello world i am thread 1" );
        }
    } );
    threads.emplace_back( [ & ]() {
        while ( 1 ) {
            logger.info( "test log %s\n", "hello world i am thread 2" );
        }
    } );
    threads.emplace_back( [ & ]() {
        while ( 1 ) {
            logger.info( "test log %s\n", "hello world i am thread 3" );
        }
    } );
    threads.emplace_back( [ & ]() {
        while ( 1 ) {
            logger.info( "test log %s\n", "hello world i am thread 4" );
        }
    } );

    while ( 1 ) {}
    return 0;
}
