cmake_minimum_required(VERSION 3.6)
project(HWMonitor)

add_definitions(-D_DEFAULT_SOURCE)
#add_definitions(-D_GNU_SOURCE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -O0 -ggdb3  -pedantic-errors -Wall -Wpedantic -Wextra -Wno-unused-function -Wno-unused-parameter -Werror -pipe")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")

set(SOURCE_FILES main.c)
add_executable(HWMonitor ${SOURCE_FILES})
target_link_libraries(HWMonitor blkid ncurses pthread mcheck)