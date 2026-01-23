cmake_minimum_required(VERSION 3.5...4.0)
project(logger)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUT_PATH ${PROJECT_SOURCE_DIR}/./bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_PATH})

aux_source_directory(./src src)
aux_source_directory(./src/core src_core)
aux_source_directory(./src/slinks src_slinks)
aux_source_directory(./src/utils src_utils)
add_executable(${PROJECT_NAME} ./test/test_log.cc ${src} ${src_core} ${src_slinks})
target_include_directories(${PROJECT_NAME}
                           PRIVATE ${PROJECT_SOURCE_DIR}/include)
