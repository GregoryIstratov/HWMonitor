/*************************************************************************************************************
    This file is part of HWMonitor.

    HWMonitor is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    HWMonitor is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HWMonitor.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************************************/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

//============================================================================================================
// SETTINGS
//============================================================================================================

#define HW_VERSION_MAJOR 0
#define HW_VERSION_MINOR_A 0
#define HW_VERSION_MINOR_B 5

#define STRING_INIT_BUFFER 4

#define DA_MAX_MULTIPLICATOR 4

#define DEVICE_BASE_SAMPLE_RATE 0.01

//============================================================================================================
// GLOBALS
//============================================================================================================
#define PTR_TO_U64(ptr) (u64)(u64*)(ptr)
#define SAFE_RELEASE(x) { if(x){ zfree(x); (x) = NULL; } }
#define CHECK_RETURN(x) { if((x) != ST_OK) { LOG_ERROR("%s returns not ok", (#x)); } }

#define KiB 1024UL
#define MiB 1048576UL
#define GiB 1073741824UL


//global return codes
enum {
    ST_OK = 0,
    ST_ERR,
    ST_NOT_FOUND,
    ST_EMPTY,
    ST_EXISTS,
    ST_OUT_OF_RANGE,
    ST_SIZE_EXCEED,
    ST_UNKNOWN
};

// global int defines, you should preffer 64 bit unsigned int
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef _Atomic u64 atomic_u64;

// global return code type
typedef u32 ret_t;


typedef void(* data_release_cb)(void* p);
