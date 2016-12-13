cmake_minimum_required(VERSION 3.6)
project(HWMonitor)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99 -march=native -mtune=native -O0 -g -pipe -Wall -Wpedantic -Wextra")

set(SOURCE_FILES main.c)
add_executable(HWMonitor ${SOURCE_FILES})
target_link_libraries(HWMonitor blkid ncurses)