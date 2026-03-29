#include "jzlog/utils/fixed_buffer.h"
#include <iostream>
#include <string>

using namespace jzlog;

int  test_pass = 0;
int  test_fail = 0;
void test_append_str() {
    utils::FixedBuffer< 10 > buffer;

    std::string str{ "hello\n" };
    buffer.append( str.c_str(), str.size() );
    if ( buffer.length() != str.size() ) {
        ++test_fail;
        std::cout << "test_append_str failed(buf is avalid)" << std::endl;
    } else {
        ++test_pass;
    }

    utils::FixedBuffer< 10 > buffer1;

    std::string str1{ "hello world\n" };
    buffer1.append( str1.c_str(), str1.size() );
    if ( buffer1.length() == 0 ) {
        ++test_pass;
    } else {
        std::cout << "test_append_str failed(buf is not avalid)" << std::endl;
        ++test_fail;
    }
}
void test_append_ch() {
    utils::FixedBuffer< 1 > buffer;
    buffer.append( 'a' );

    if ( buffer.length() != 1 ) {
        ++test_fail;
        std::cout << "test_append_ch failed(buf is avalid)" << std::endl;
    } else {
        ++test_pass;
    }
    buffer.append( 'b' );
    if ( buffer.length() != 1 ) {
        ++test_fail;
        std::cout << "test_append_ch failed(buf is avalid)" << std::endl;
    } else {
        ++test_pass;
    }
}
int main( int argc, char* argv[] ) {
    std::cout << "Test fixed_buffer begin" << std::endl;
    test_append_str();
    test_append_ch();
    std::cout << "test_pass:" << test_pass << std::endl;
    std::cout << "test_fail:" << test_fail << std::endl;
    std::cout << "Test fixed_buffer end" << std::endl;
    return 0;
}
