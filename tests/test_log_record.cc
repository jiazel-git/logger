#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include <iostream>
#include <chrono>
#include <thread>

int main( int argc, char* argv[] ) {
    jzlog::LogRecord record;
    record._level       = jzlog::loglevel::LogLevel::INFO;
    record._logger_name = "test";
    record._message     = "hello world";
    record._timestamp   = std::chrono::system_clock::now();
    record._thread_id   = std::this_thread::get_id();

    std::cout << "LogRecord test passed" << std::endl;
    return 0;
}
