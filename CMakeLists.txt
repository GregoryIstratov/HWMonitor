cmake_minimum_required(VERSION 3.6)

project(HWMonitor)

add_definitions(-D_GNU_SOURCE)
#add_definitions(-DHW_NO_SLEEP)

#set(VALGRIND_ENABLE 1)

set(SOURCE_FILES main.c globals.h log.h log.c concurrent_hashtable.h concurrent_hashtable.c crc64.h crc64.c allocators.c allocators.h dynamic_allocator.c dynamic_allocator.h double_linked_list.c double_linked_list.h string.c string.h lifo.c lifo.h fifo.c fifo.h timer.c timer.h vector.c vector.h binary_tree.c binary_tree.h tests.c tests.h utils.c utils.h blk_dev.c blk_dev.h net_dev.c net_dev.h cpu_dev.c cpu_dev.h mem_dev.c mem_dev.h)

if(NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    message(FATAL_ERROR "Primary compiler is Clang, other compilers is not supported")
endif()

set(DIAGNOSTIC_FLAGS "-Weverything -Werror -Wno-unused-function -Wno-unused-macros -Wno-unused-parameter -Wno-format-nonliteral")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    message("Building Debug verision...")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native -std=gnu11 -O0 -g  ${DIAGNOSTIC_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    add_executable(HWMonitor ${SOURCE_FILES})
else()
    message("Building Release verision...")
    if (${VALGRIND_ENABLE})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native -std=gnu11 -Og -funroll-loops ${DIAGNOSTIC_FLAGS}")
        add_executable(HWMonitor ${SOURCE_FILES})
    else ()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native -std=gnu11 -O3 -funroll-loops ${DIAGNOSTIC_FLAGS}")
        add_executable(HWMonitor ${SOURCE_FILES})
        SET_TARGET_PROPERTIES(HWMonitor PROPERTIES LINK_FLAGS -Wl,-s)
    endif ()

endif()


target_link_libraries(HWMonitor ncurses pthread)

