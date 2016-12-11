cmake_minimum_required(VERSION 3.6)
project(HWMonitor)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -march=native -mtune=native -Os -pipe -Wall -Wpedantic -Wextra")

set(SOURCE_FILES main.cpp)
add_executable(HWMonitor ${SOURCE_FILES})
target_link_libraries(HWMonitor blkid ncurses)