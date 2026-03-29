cmake_minimum_required(VERSION 3.5...4.0)
project(
  jzlog
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 输出目录配置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 收集源文件 aux_source_directory(./src/core SRC_CORE)
aux_source_directory(./src/sinks SRC_SINKS)
aux_source_directory(./src/archive_manager SRC_ARCHIVE_MANAGER)
set(JZLOG_SOURCES ${SRC_CORE} ${SRC_SINKS} ${SRC_ARCHIVE_MANAGER})

# 创建库
add_library(jzlog STATIC ${JZLOG_SOURCES})
target_include_directories(jzlog PUBLIC ${PROJECT_SOURCE_DIR}/include)

# 添加编译选项
target_compile_features(jzlog PUBLIC cxx_std_17)

# 测试可执行文件
add_executable(test_log ./tests/test_log.cc)
target_link_libraries(test_log PRIVATE jzlog)

add_executable(test_log_level ./tests/test_log_level.cc)
target_link_libraries(test_log_level PRIVATE jzlog)

add_executable(test_log_record ./tests/test_log_record.cc)
target_link_libraries(test_log_record PRIVATE jzlog)

add_executable(test_fixed_buff ./tests/test_fixed_buffer.cc)
target_link_libraries(test_fixed_buff PRIVATE jzlog)

# 示例可执行文件
add_executable(basic_usage ./examples/basic_usage.cc)
target_link_libraries(basic_usage PRIVATE jzlog)
