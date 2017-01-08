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

#include "globals.h"

//============================================================================================================
// LOG
//============================================================================================================

#define ENABLE_LOGGING

#define LOG_SHOW_TIME
#define LOG_SHOW_DATE
#define LOG_SHOW_THREAD
#define LOG_SHOW_PATH
#define LOG_ENABLE_MULTITHREADING

#define LOG_FORMAT_BUFFER_MAX_SIZE 12400

enum {
    LOGLEVEL_NONE = 0,
    LOGLEVEL_WARN,
    LOGLEVEL_INFO,
    LOGLEVEL_DEBUG,
    LOGLEVEL_TRACE,
    LOGLEVEL_ALL = 0xFFFFFF
};

enum {
    LOG_ERROR = 0,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_ASSERT

};

#ifndef NDEBUG
#define LOG_ERROR(...) (_log(__FILE__, __LINE__, __func__, LOG_ERROR, __VA_ARGS__))
#define LOG_WARN(...) (_log(__FILE__, __LINE__, __func__, LOG_WARN, __VA_ARGS__))
#define LOG_DEBUG(...) (_log(__FILE__, __LINE__, __func__, LOG_DEBUG, __VA_ARGS__))
#define LOG_INFO(...) (_log(__FILE__, __LINE__, __func__, LOG_INFO, __VA_ARGS__))
#define LOG_TRACE(...) (_log(__FILE__, __LINE__, __func__, LOG_TRACE, __VA_ARGS__))
#define LOG_ASSERT(...) (_log(__FILE__, __LINE__, __func__, LOG_ASSERT, __VA_ARGS__))
#define ASSERT(exp) ((exp)?((void)0): _log(__FILE__, __LINE__, __func__, LOG_ASSERT, #exp))
#define ASSERT_EQ(a, b) ((a == b)?((void)0): LOG_ASSERT("%s != %s [%lu] != [%lu]", #a, #b, a, b))
#else
#define LOG_ERROR(...) (_log(__FILE__, __LINE__, __func__, LOG_ERROR, __VA_ARGS__))
#define LOG_WARN(...) (_log(__FILE__, __LINE__, __func__, LOG_WARN, __VA_ARGS__))
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...) (_log(__FILE__, __LINE__, __func__, LOG_INFO, __VA_ARGS__))
#define LOG_TRACE(...) ((void)0)
#define LOG_ASSERT(...) ((void)0)
#define ASSERT(exp) ((void)0)
#define ASSERT_EQ(a, b) ((void)0)
#endif


void log_init(u64 loglvl, const char* filename);

void log_shutdown(void);


void _log(const char* file, u64 line, const char* fun, u64 lvl, ...);
