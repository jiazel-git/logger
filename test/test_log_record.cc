#include "jzlog/core/log_builder.h"
#include "jzlog/core/log_level.h"
#include <iostream>
#include <utility>
int main(int argc, char* argv[]) {
    jzlog::CLogBuilder log_builder{jzlog::LogLevel::INFO, "builder1"};

    auto log_builder1{std::move(log_builder)};
    std::cout << log_builder1.to_string();
    return 0;
}
