cmake_minimum_required(VERSION 2.8)
project(HWMonitor)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_DEFAULT_SOURCE -std=c99 -O0 -g  -pedantic-errors -Wall -Wpedantic -Wextra -Wno-unused-function -Wno-unused-parameter -Werror -pipe")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")

set(SOURCE_FILES main.c)
add_executable(HWMonitor ${SOURCE_FILES})
target_link_libraries(HWMonitor blkid ncurses pthread)