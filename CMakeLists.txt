cmake_minimum_required(VERSION 3.6)
project(HWMonitor)

add_definitions(-D_DEFAULT_SOURCE)
add_definitions(-D_GNU_SOURCE)

#set(VALGRIND_ENABLE 1)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    message("Building Debug verision...")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -O0 -ggdb3 -pedantic-errors -Wall -Wpedantic -Wextra -Wno-unused-function -Wno-unused-parameter -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
else()
    message("Building Release verision...")
    if (${VALGRIND_ENABLE})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native -std=c11 -O2 -g -funroll-loops -pedantic-errors -Wall -Wpedantic -Wextra -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Werror")
    else ()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native -std=c11 -Ofast -funroll-loops -pedantic-errors -Wall -Wpedantic -Wextra -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Werror")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--gc-sections -Wl,--strip-all")
    endif ()

endif()

set(SOURCE_FILES main.c)
add_executable(HWMonitor ${SOURCE_FILES})
target_link_libraries(HWMonitor ncurses pthread)