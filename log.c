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

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "log.h"

#define LOG_RED   "\x1B[31m"
#define LOG_GRN   "\x1B[32m"
#define LOG_YEL   "\x1B[33m"
#define LOG_BLU   "\x1B[34m"
#define LOG_MAG   "\x1B[35m"
#define LOG_CYN   "\x1B[36m"
#define LOG_WHT   "\x1B[37m"
#define LOG_RESET "\x1B[0m"

#ifdef LOG_ENABLE_MULTITHREADING
static pthread_spinlock_t stdlog_spinlock;
#endif

static u64 loglevel = LOGLEVEL_DEBUG;
static FILE* stdlog = NULL;

void log_init(u64 loglvl, const char* filename) {
    loglevel = loglvl;
#ifdef LOG_ENABLE_MULTITHREADING
    pthread_spin_init(&stdlog_spinlock, 0);
#endif
    stdlog = fopen(filename, "a");
}

void log_shutdown() {
    if (stdlog)
        fclose(stdlog);

#ifdef LOG_ENABLE_MULTITHREADING
    pthread_spin_destroy(&stdlog_spinlock);
#endif
}

static const char* loglevel_s(u64 lvl) {
    switch (lvl) {
        case LOG_ERROR:
            return "ERR";
        case LOG_WARN:
            return "WRN";
        case LOG_DEBUG:
            return "DBG";
        case LOG_INFO:
            return "INF";
        case LOG_TRACE:
            return "TRC";
        case LOG_ASSERT:
            return "ASSERTION FAILED";
        default:
            return "UNKNOWN";
    }
}

static const char* log_color(u64 lvl) {
    switch (lvl) {
        case LOG_ERROR:
            return LOG_RED;
        case LOG_DEBUG:
            return LOG_CYN;
        case LOG_INFO:
            return LOG_GRN;
        case LOG_TRACE:
            return LOG_WHT;
        case LOG_WARN:
            return LOG_YEL;
        case LOG_ASSERT:
            return LOG_YEL;
        default:
            return LOG_RESET;
    }
}

void _log(const char* file, u64 line, const char* fun, u64 lvl, ...) {
    if (lvl <= loglevel) {

#ifdef LOG_SHOW_THREAD
        pid_t tid = (pid_t)syscall(__NR_gettid);
#endif

        va_list args;
        va_start(args, lvl);
        const char* fmt = va_arg(args, const char*);

        char buf[LOG_FORMAT_BUFFER_MAX_SIZE];
        memset(buf, 0, LOG_FORMAT_BUFFER_MAX_SIZE);
        vsnprintf(buf, LOG_FORMAT_BUFFER_MAX_SIZE, fmt, args);

        va_end(args);

#ifdef LOG_ENABLE_MULTITHREADING
        pthread_spin_lock(&stdlog_spinlock);
#endif

#if defined LOG_SHOW_TIME || defined LOG_SHOW_DATE
        time_t t;
        struct tm _tml;
        struct tm* tml;
        if (time(&t) == (time_t)-1) {
            LOG_ERROR("time return failed");
            return;
        }

        localtime_r(&t, &_tml);
        tml = &_tml;

#endif

        fprintf(stdlog, "%s", log_color(lvl));
#ifdef LOG_SHOW_TIME
        fprintf(stdlog, "[%02d:%02d:%02d]", tml->tm_hour, tml->tm_min, tml->tm_sec);
#endif
#ifdef LOG_SHOW_DATE
        fprintf(stdlog, "[%02d/%02d/%d]", tml->tm_mday, tml->tm_mon + 1, tml->tm_year - 100);
#endif
#ifdef LOG_SHOW_THREAD
        fprintf(stdlog, "[0x%08X]", tid);
#endif

        fprintf(stdlog, "[%s][%s]: %s%s", loglevel_s(lvl), fun, buf, LOG_RESET);

#ifdef LOG_SHOW_PATH
        fprintf(stdlog, " - %s:%lu", file, line);
#endif
        fprintf(stdlog, "\n");
        fflush(stdlog);

#ifdef LOG_ENABLE_MULTITHREADING
        pthread_spin_unlock(&stdlog_spinlock);
#endif

    }
}
