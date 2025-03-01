
cmake_minimum_required(VERSION 3.5...4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUT_PATH ${PROJECT_SOURCE_DIR}/./bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_PATH})

include_directories(./include)
add_executable(main main.cc)