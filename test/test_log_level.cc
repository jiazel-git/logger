#include "jzlog/core/log_level.h"
#include <iostream>
using namespace jzlog;
int main( int argc, char* argv[] ) {
    loglevel::LogLevel level{ loglevel::LogLevel::ALL };
    std::cout << "level=" << static_cast< int >( level ) << " 1";
    return 0;
}
