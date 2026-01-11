cmake_minimum_required(VERSION 3.5...4.0)
project(logger)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUT_PATH ${PROJECT_SOURCE_DIR}/./bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_PATH})

aux_source_directory(./src/core src_core)
add_executable(${PROJECT_NAME} ./test/test_log_record.cc ${src_core})
target_include_directories(${PROJECT_NAME}
                           PRIVATE ${PROJECT_SOURCE_DIR}/include)
message(STATUS "src_core = ${src_core}")
