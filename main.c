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

//============================================================================================================
// INCLUDES
//============================================================================================================

#include <ncurses.h>     // libncurses5-dev
#include <sys/param.h>
#include <stdlib.h>
#include <memory.h>
#include <locale.h>

#include "globals.h"
#include "log.h"
#include "concurrent_hashtable.h"
#include "allocators.h"
#include "double_linked_list.h"
#include "string.h"
#include "timer.h"
#include "tests.h"
#include "blk_dev.h"
#include "net_dev.h"
#include "utils.h"
#include "mem_dev.h"
#include "cpu_dev.h"


//============================================================================================================
// GLOBALS
//============================================================================================================

static atomic_bool programm_exit = false;
static list_t* ldevices = NULL;
static pthread_mutex_t ldevices_mtx;

static list_t* lnet_devs = NULL;
static pthread_mutex_t lnet_devs_mtx;

static cpu_info_t* g_cpu_info = NULL;
static pthread_mutex_t cpu_info_mtx;

static mem_info_t* g_mem_info = NULL;
static pthread_mutex_t mem_info_mtx;

static atomic_u64 sample_rate_mul = 100;
static atomic_u64 cpu_usage = 0;

static u64 g_nframe = 0;

static inline double device_get_sample_rate() {
    return DEVICE_BASE_SAMPLE_RATE * atomic_load(&sample_rate_mul);
}

//============================================================================================================
// GUI
//============================================================================================================
#define NCOLOR_PAIR_WHITE_ON_BLACK  1
#define NCOLOR_PAIR_GREEN_ON_BLACK  2
#define NCOLOR_PAIR_CYAN_ON_BLACK   3
#define NCOLOR_PAIR_YELLOW_ON_BLACK 4
#define NCOLOR_PAIR_RED_ON_BLACK    5

#define COLON_OFFSET 1
#define COLON_DEVICE COLON_OFFSET
#define COLON_READ  (9 + COLON_OFFSET)
#define COLON_WRITE (22 + COLON_OFFSET)
#define COLON_SIZE (35 + COLON_OFFSET)
#define COLON_USE  (46 + COLON_OFFSET)
#define COLON_PERC (57 + COLON_OFFSET)
#define COLON_FILESYSTEM (64 + COLON_OFFSET)
#define COLON_SCHED (70 + COLON_OFFSET)
#define COLON_MOUNT (75 + COLON_OFFSET)
#define COLON_MODEL (83 + COLON_OFFSET)

#define COLON_NET_NAME (COLON_DEVICE)
#define COLON_NET_READ (COLON_READ)
#define COLON_NET_WRITE (COLON_WRITE)
#define COLON_NET_MTU (COLON_PERC-3)
#define COLON_NET_SPEED (COLON_USE-3)
#define COLON_NET_PERC (COLON_SIZE)

static void* ncurses_keypad(void* p) {
    int c;
    while (true) {
        c = wgetch(stdscr);
        switch (c) {
            case KEY_F(10):
                atomic_store(&programm_exit, true);
                return p;
            case KEY_UP:
                atomic_fetch_add(&sample_rate_mul, 1);
                break;
            case KEY_DOWN: {
                while (true) {
                    u64 mul = atomic_load(&sample_rate_mul);
                    if (mul > 1) {
                        if (atomic_compare_exchange_strong(&sample_rate_mul, &mul, mul - 1))
                            break;
                    } else {
                        break;
                    }
                }

                break;
            }

            default:
                break;
        }
    }
}

static void ncruses_print_hr(int row, int col, u64 value) {
    double size = 0.0;
    int type = 0;
    human_readable_size(value, &size, &type);

    char buffer[128] = {0};
    switch (type) {
        case HR_SIZE_KB:
            sprintf(buffer, "%06.2f Kb", size);
            break;
        case HR_SIZE_MB:
            sprintf(buffer, "%06.2f Mb", size);
            break;
        case HR_SIZE_GB:
            sprintf(buffer, "%06.2f Gb", size);
            break;
        default:
            break;
    }

    mvaddstr(row, col, buffer);
}

static void ncruses_print_hr_speed(int row, int col, double bytes, double green_barier) {

    double r = bytes / 1024.0;
    if (r < 1024)  // KB / sec
    {
        char s[64] = {0};
        sprintf(s, "%06.2f Kb/s", r);

        if (r > 0) {
            attron(COLOR_PAIR(NCOLOR_PAIR_GREEN_ON_BLACK));
            mvaddstr(row, col, s);
            attroff(COLOR_PAIR(NCOLOR_PAIR_GREEN_ON_BLACK));
        } else {
            attron(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));
            mvaddstr(row, col, s);
            attroff(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));
        }

        return;
    }

    r = bytes / 1024 / 1024;
    if (r < 1024)  // MiB / sec
    {
        char s[64] = {0};
        sprintf(s, "%06.2f Mb/s", r);

        attron(A_BOLD);
        if (r < green_barier) {
            attron(COLOR_PAIR(NCOLOR_PAIR_GREEN_ON_BLACK));
            mvaddstr(row, col, s);
            attroff(COLOR_PAIR(NCOLOR_PAIR_GREEN_ON_BLACK));
        } else if (r < (green_barier * 3)) {
            attron(COLOR_PAIR(NCOLOR_PAIR_YELLOW_ON_BLACK));
            mvaddstr(row, col, s);
            attroff(COLOR_PAIR(NCOLOR_PAIR_YELLOW_ON_BLACK));
        } else {
            attron(COLOR_PAIR(NCOLOR_PAIR_RED_ON_BLACK));
            mvaddstr(row, col, s);
            attroff(COLOR_PAIR(NCOLOR_PAIR_RED_ON_BLACK));
        }
        attroff(A_BOLD);
        return;
    }

    r = bytes / 1024 / 1024 / 1024;
    {
        char s[64] = {0};
        sprintf(s, "%06.2f Gb/s", r);

        attron(A_BOLD);
        attron(COLOR_PAIR(NCOLOR_PAIR_RED_ON_BLACK));
        mvaddstr(row, col, s);
        attroff(COLOR_PAIR(NCOLOR_PAIR_RED_ON_BLACK));
        attroff(A_BOLD);

        return;
    }
}

static const char* animation_bug() {
    static u64 frame = 0;
    static bool way = true;
    static const char* movie[] = {
"\xE2\x98\x83______________________________________________________________________________________________",
"__\xE2\x98\x83____________________________________________________________________________________________",
"____\xE2\x98\x83__________________________________________________________________________________________",
"______\xE2\x98\x83________________________________________________________________________________________",
"________\xE2\x98\x83______________________________________________________________________________________",
"__________\xE2\x98\x83____________________________________________________________________________________",
"____________\xE2\x98\x83__________________________________________________________________________________",
"______________\xE2\x98\x83________________________________________________________________________________",
"________________\xE2\x98\x83______________________________________________________________________________",
"__________________\xE2\x98\x83____________________________________________________________________________",
"____________________\xE2\x98\x83__________________________________________________________________________",
"______________________\xE2\x98\x83________________________________________________________________________",
"________________________\xE2\x98\x83______________________________________________________________________",
"__________________________\xE2\x98\x83____________________________________________________________________",
"____________________________\xE2\x98\x83__________________________________________________________________",
"______________________________\xE2\x98\x83________________________________________________________________",
"________________________________\xE2\x98\x83______________________________________________________________",
"__________________________________\xE2\x98\x83____________________________________________________________",
"____________________________________\xE2\x98\x83__________________________________________________________",
"______________________________________\xE2\x98\x83________________________________________________________",
"________________________________________\xE2\x98\x83______________________________________________________",
"__________________________________________\xE2\x98\x83____________________________________________________",
"____________________________________________\xE2\x98\x83__________________________________________________",
"______________________________________________\xE2\x98\x83________________________________________________",
"________________________________________________\xE2\x98\x83______________________________________________",
"__________________________________________________\xE2\x98\x83____________________________________________",
"____________________________________________________\xE2\x98\x83__________________________________________",
"______________________________________________________\xE2\x98\x83________________________________________",
"________________________________________________________\xE2\x98\x83______________________________________",
"__________________________________________________________\xE2\x98\x83____________________________________",
"____________________________________________________________\xE2\x98\x83__________________________________",
"______________________________________________________________\xE2\x98\x83________________________________",
"________________________________________________________________\xE2\x98\x83______________________________",
"__________________________________________________________________\xE2\x98\x83____________________________",
"____________________________________________________________________\xE2\x98\x83__________________________",
"______________________________________________________________________\xE2\x98\x83________________________",
"________________________________________________________________________\xE2\x98\x83______________________",
"__________________________________________________________________________\xE2\x98\x83____________________",
"____________________________________________________________________________\xE2\x98\x83__________________",
"______________________________________________________________________________\xE2\x98\x83________________",
"________________________________________________________________________________\xE2\x98\x83______________",
"__________________________________________________________________________________\xE2\x98\x83____________",
"____________________________________________________________________________________\xE2\x98\x83__________",
"______________________________________________________________________________________\xE2\x98\x83________",
"________________________________________________________________________________________\xE2\x98\x83______",
"__________________________________________________________________________________________\xE2\x98\x83____",
"____________________________________________________________________________________________\xE2\x98\x83__",
"______________________________________________________________________________________________\xE2\x98\x83",
    };

    u64 msize = sizeof(movie)/8-1;
    if (frame >= msize)
        way = false;

    if (frame == 0)
        way = true;

    if (way)
        return movie[frame++];
    else
        return movie[frame--];
}

//@progress - 0-50
static void ncurses_bar_render(int row, int col, int64_t progress) {
    const char* lit = "❯";

    string* gs = NULL;
    string_init(&gs);
    string* ys = NULL;
    string_init(&ys);
    string* rs = NULL;
    string_init(&rs);

    for (int64_t i = 0; i < MIN(progress, 30); ++i) {
        string_append(gs, lit);
    }

    for (int64_t i = 0; i < MIN(progress - 30, 15); ++i) {
        string_append(ys, lit);
    }

    for (int64_t i = 0; i < MIN(progress - 45, 5); ++i) {
        string_append(rs, lit);
    }

    char* gbar = string_makez(gs);
    string_release(gs);

    char* ybar = string_makez(ys);
    string_release(ys);

    char* rbar = string_makez(rs);
    string_release(rs);

    mvaddstr(row, col, "[");
    attron(COLOR_PAIR(2));
    mvaddstr(row, col + 1, gbar);
    attroff(COLOR_PAIR(2));
    attron(COLOR_PAIR(4));
    mvaddstr(row, col + 31, ybar);
    attroff(COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    mvaddstr(row, col + 46, rbar);
    attroff(COLOR_PAIR(5));

    mvaddstr(row, col + 51, "]");

    zfree(gbar);
    zfree(ybar);
    zfree(rbar);
}

static void ncurses_cpu_bar_render(int row, int col) {

    ulong cpus = atomic_load(&cpu_usage);
    int64_t cu = (int64_t)cpus / 2;

    ncurses_bar_render(row, col, cu);

    char load_s[32];
    sprintf(load_s, "%02lu%% CPU", cpus);
    mvaddstr(row, col + 53, load_s);

}

static void ncurses_addstrf(int row, int col, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buf[2048];
    memset(buf, 0, 2048);
    vsnprintf(buf, 2048, fmt, args);

    va_end(args);

    mvaddstr(row, col, buf);
}

static void ncurses_window() {
    initscr();            /* Start curses mode 		  */

    if (has_colors() == FALSE) {
        endwin();
        LOG_ERROR("Your terminal does not support color");
        exit(EXIT_FAILURE);
    }

    raw();
    keypad(stdscr, TRUE);
    noecho();
    start_color();
    curs_set(0); //invisible cursor

    pthread_t keypad__thrd;
    pthread_create(&keypad__thrd, NULL, &ncurses_keypad, NULL);
    pthread_setname_np(keypad__thrd, "keypad");
    pthread_detach(keypad__thrd);

    init_pair(NCOLOR_PAIR_WHITE_ON_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(NCOLOR_PAIR_GREEN_ON_BLACK, COLOR_GREEN, COLOR_BLACK);
    init_pair(NCOLOR_PAIR_CYAN_ON_BLACK, COLOR_CYAN, COLOR_BLACK);
    init_pair(NCOLOR_PAIR_YELLOW_ON_BLACK, COLOR_YELLOW, COLOR_BLACK);
    init_pair(NCOLOR_PAIR_RED_ON_BLACK, COLOR_RED, COLOR_BLACK);

    double frame_time = 1.0;
    while (!atomic_load(&programm_exit)) {
        struct timespec tm_start = timer_start();

        double frame_rate = 1.0 / device_get_sample_rate();
        u64 scr_upd = (u64)(NANOSEC_IN_SEC / frame_rate);
        int row = 1;
        ++g_nframe;

        clear();

        attron(A_BOLD);
        attron(COLOR_PAIR(NCOLOR_PAIR_WHITE_ON_BLACK));

        mvaddstr(row++, 1, animation_bug());
        row++;

        char hwversion_s[128] = {0};
        sprintf(hwversion_s, "HWMonitor %d.%d%d", HW_VERSION_MAJOR, HW_VERSION_MINOR_A,
                HW_VERSION_MINOR_B);
        mvaddstr(row++, 1, hwversion_s);

        mvaddstr(row++, 1,
                 "Keypad: [UP - Increase sample rate][DOWN - Decrease sample rate][F10 Exit]");

        char samplesize_s[128] = {0};
        sprintf(samplesize_s, "Sample rate %05.3f sec", device_get_sample_rate());
        mvaddstr(row++, 1, samplesize_s);

        ncurses_addstrf(row++, 1, "Frame time: %.3f ms", frame_time);
        ncurses_addstrf(row++, 1, "FPS: %.2f", (1000.0 / frame_time));

        row++;
        mvaddstr(row++, 1,
                 "_______________________________________________________________________________________________");
        row++;

        pthread_mutex_lock(&cpu_info_mtx);

        if (g_cpu_info) {

            char* cpu_name = string_makez(g_cpu_info->name);
            char* cpu_clock = string_makez(g_cpu_info->clock);

            ncurses_addstrf(row++, 1, "%dx %s (%s MHz)", g_cpu_info->cores, cpu_name, cpu_clock);

            zfree(cpu_clock);
            zfree(cpu_name);
        }

        pthread_mutex_unlock(&cpu_info_mtx);

        row++;

        attroff(COLOR_PAIR(NCOLOR_PAIR_WHITE_ON_BLACK));
        ncurses_cpu_bar_render(row++, 1);
        attron(COLOR_PAIR(NCOLOR_PAIR_WHITE_ON_BLACK));

        pthread_mutex_lock(&mem_info_mtx);

        if (g_mem_info) {

            double mem_load_perc = 100.0 - g_mem_info->mem_free / (double)g_mem_info->mem_total * 100.0;
            int64_t mem_load = (int64_t)(mem_load_perc / 2.0);

            ncurses_bar_render(row, 1, mem_load);
            u64 mem_total = g_mem_info->mem_total / 1024 / 1024;
            u64 mem_used = (g_mem_info->mem_total - g_mem_info->mem_free) / 1024 / 1024;
            char load_s[64];
            sprintf(load_s, "%02lu%% Memory [%lu/%lu Mb]", (ulong)mem_load_perc, mem_used, mem_total);
            mvaddstr(row++, 54, load_s);

            double swap_load_perc = 100.0 - g_mem_info->swap_free / (double)g_mem_info->swap_total * 100.0;
            int64_t swap_load = (int64_t)(swap_load_perc / 2.0);

            ncurses_bar_render(row, 1, swap_load);
            u64 swap_total = g_mem_info->swap_total / 1024 / 1024;
            u64 swap_used = (g_mem_info->swap_total - g_mem_info->swap_free) / 1024 / 1024;
            char sload_s[64];
            sprintf(sload_s, "%02lu%% Swap   [%lu/%lu Mb]", (ulong)swap_load_perc, swap_used, swap_total);
            mvaddstr(row++, 54, sload_s);

        }

        pthread_mutex_unlock(&mem_info_mtx);

        mvaddstr(row++, 1,
                 "_______________________________________________________________________________________________");

        mvaddstr(++row, COLON_DEVICE, "Device");
        mvaddstr(row, COLON_READ, "Read");
        mvaddstr(row, COLON_WRITE, "Write");
        mvaddstr(row, COLON_SIZE, "Size");
        mvaddstr(row, COLON_USE, "Use");
        mvaddstr(row, COLON_PERC, "%");
        mvaddstr(row, COLON_FILESYSTEM, "FS");
        mvaddstr(row, COLON_SCHED, "Sch");
        mvaddstr(row, COLON_MOUNT, "Mount");
        mvaddstr(row++, COLON_MODEL, "Model");
        attroff(COLOR_PAIR(NCOLOR_PAIR_WHITE_ON_BLACK));

        attroff(A_BOLD);
        pthread_mutex_lock(&ldevices_mtx);

        if (ldevices) {

            list_iter_t* it = NULL;
            list_iter_init(ldevices, &it);
            blk_dev_t* dev = NULL;
            while ((dev = list_iter_next(it))) {
                attron(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));

                char* name = string_makez(dev->name);
                char* fs = string_makez(dev->fs);
                char* model = string_makez(dev->model);
                char* mount = string_makez(dev->mount);

                char* shed = string_makez(dev->shed);

                char perc[32] = {0};
                sprintf(perc, "%04.1f%%", dev->perc);

                mvaddstr(row, COLON_DEVICE, name);

                attroff(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));
                ncruses_print_hr_speed(row, COLON_READ, dev->perf_read, 100.);
                ncruses_print_hr_speed(row, COLON_WRITE, dev->perf_write, 100.);
                attron(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));

                ncruses_print_hr(row, COLON_SIZE, dev->size);
                ncruses_print_hr(row, COLON_USE, dev->used);
                mvaddstr(row, COLON_PERC, perc);
                mvaddstr(row, COLON_FILESYSTEM, fs);
                mvaddstr(row, COLON_SCHED, shed);
                mvaddstr(row, COLON_MOUNT, mount);
                mvaddstr(row++, COLON_MODEL, model);

                zfree(shed);
                zfree(mount);
                zfree(model);
                zfree(fs);
                zfree(name);

                attroff(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));
            }

            list_iter_release(it);
        }

        pthread_mutex_unlock(&ldevices_mtx);

        attron(A_BOLD);

        row++;
        mvaddstr(row++, 1,
                 "_______________________________________________________________________________________________");
        mvaddstr(++row, COLON_NET_NAME, "Device");
        mvaddstr(row, COLON_NET_READ, "RX");
        mvaddstr(row, COLON_NET_WRITE, "TX");
        mvaddstr(row, COLON_NET_MTU, "MTU");
        mvaddstr(row, COLON_NET_SPEED, "Speed");
        mvaddstr(row, COLON_NET_PERC, "%");

        attroff(A_BOLD);
        pthread_mutex_lock(&lnet_devs_mtx);

        if (lnet_devs) {

            list_iter_t* it = NULL;
            list_iter_init(lnet_devs, &it);
            net_dev_t* ndev = NULL;
            while ((ndev = list_iter_next(it))) {
                attron(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));

                char* name = string_makez(ndev->name);
                char* mtu = string_makez(ndev->mtu);
                char* speed = string_makez(ndev->speed);

                char perc[64] = {0};
                sprintf(perc, "%04.1f%%", ndev->bandwidth_use);

                mvaddstr(++row, COLON_NET_NAME, name);
                attroff(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));
                ncruses_print_hr_speed(row, COLON_NET_READ, ndev->rx_speed, 1.5);
                ncruses_print_hr_speed(row, COLON_NET_WRITE, ndev->tx_speed, 1.5);
                attron(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));
                mvaddstr(row, COLON_NET_MTU, mtu);
                mvaddstr(row, COLON_NET_SPEED, speed);
                mvaddstr(row, COLON_NET_PERC, perc);

                zfree(speed);
                zfree(mtu);
                zfree(name);

                attroff(COLOR_PAIR(NCOLOR_PAIR_CYAN_ON_BLACK));
            }

            list_iter_release(it);

        }

        pthread_mutex_unlock(&lnet_devs_mtx);

        attroff(A_BOLD);

        refresh(); // Print to the screen
#ifndef HW_NO_SLEEP
        nsleep((u64)scr_upd);
#endif

        frame_time = timer_end_ms(tm_start);
    }

    endwin();
}

//============================================================================================================
// BLACK DEV RUN
//============================================================================================================

static void blk_dev_set_globals(list_t* devs)
{
    pthread_mutex_lock(&ldevices_mtx);
    if (ldevices)
        list_release(ldevices, true);
    ldevices = devs;
    pthread_mutex_unlock(&ldevices_mtx);
}

static void* start_blkdev_sample(void* p) {
    while (!atomic_load(&programm_exit)) {

        double sample_rate = device_get_sample_rate();
        blkdev_sample(sample_rate, &blk_dev_set_globals);
    }

    return p;
}


//============================================================================================================
// NET DEV RUN
//============================================================================================================

static void net_dev_set_globals(list_t* devs)
{
    pthread_mutex_lock(&lnet_devs_mtx);
    if (lnet_devs)
        list_release(lnet_devs, true);
    lnet_devs = devs;
    pthread_mutex_unlock(&lnet_devs_mtx);
}

static void* start_net_dev_sample(void* p) {
    while (!atomic_load(&programm_exit)) {
        double sample_rate = device_get_sample_rate();

        net_dev_sample(sample_rate, &net_dev_set_globals);
    }

    return p;
}

//============================================================================================================
// CPU PROC SAMPLING
//============================================================================================================
static void cpu_dev_sample(double sample_size_sec) {
    cpu_dev_t* cpu_a = NULL;
    cpu_dev_t* cpu_b = NULL;

    pthread_mutex_lock(&cpu_info_mtx);

    if (g_cpu_info) {
        cpu_info_release_cb(g_cpu_info);
        g_cpu_info = NULL;
    }

    cpu_info_get(&g_cpu_info);

    pthread_mutex_unlock(&cpu_info_mtx);

    cpu_dev_get(&cpu_a);

#ifndef HW_NO_SLEEP
    nsleepd(sample_size_sec);
#endif

    cpu_dev_get(&cpu_b);

    double usage = cpu_dev_diff_usage(cpu_a, cpu_b) * 100.0;

    atomic_store(&cpu_usage, (ulong)usage);

    cpu_dev_release_cb(cpu_a);
    cpu_dev_release_cb(cpu_b);
}

static void* start_cpu_dev_sample(void* p) {
    while (!atomic_load(&programm_exit)) {
        double sample_rate = device_get_sample_rate();

        cpu_dev_sample(sample_rate);
    }

    return p;
}

//============================================================================================================
// MEM INFO SAMPLING
//============================================================================================================
static void mem_info_sample(double sample_size_sec) {

    pthread_mutex_lock(&mem_info_mtx);

    if (g_mem_info) {
        mem_info_release_cb(g_mem_info);
        g_mem_info = NULL;
    }

    mem_info_get(&g_mem_info);

    pthread_mutex_unlock(&mem_info_mtx);

#ifndef HW_NO_SLEEP
    nsleepd(sample_size_sec);
#endif
}

static void* start_mem_info_sample(void* p) {
    while (!atomic_load(&programm_exit)) {
        double sample_rate = device_get_sample_rate();

        mem_info_sample(sample_rate);

    }

    return p;
}

//============================================================================================================
// MISC
//============================================================================================================

static void sig_handler(int signo) {
    if (signo == SIGTERM || signo == SIGINT) {
        atomic_store(&programm_exit, true);
    }

    if (signo == SIGUSR1) {
#ifndef NDEBUG
        alloc_dump_summary();
#endif
    }
}

static void check_style_defines() {
#ifdef _POSIX_SOURCE
    LOG_DEBUG("_POSIX_SOURCE defined");
#endif

#ifdef _POSIX_C_SOURCE
    LOG_DEBUG("_POSIX_C_SOURCE defined: %ldL", (long)_POSIX_C_SOURCE);
#endif

#ifdef _ISOC99_SOURCE
    LOG_DEBUG("_ISOC99_SOURCE defined");
#endif

#ifdef _ISOC11_SOURCE
    LOG_DEBUG("_ISOC11_SOURCE defined\n");
#endif

#ifdef _XOPEN_SOURCE
    LOG_DEBUG("_XOPEN_SOURCE defined: %d\n", _XOPEN_SOURCE);
#endif

#ifdef _XOPEN_SOURCE_EXTENDED
    LOG_DEBUG("_XOPEN_SOURCE_EXTENDED defined\n");
#endif

#ifdef _LARGEFILE64_SOURCE
    LOG_DEBUG("_LARGEFILE64_SOURCE defined\n");
#endif

#ifdef _FILE_OFFSET_BITS
    LOG_DEBUG("_FILE_OFFSET_BITS defined: %d\n", _FILE_OFFSET_BITS);
#endif

#ifdef _BSD_SOURCE
    LOG_DEBUG("_BSD_SOURCE defined\n");
#endif

#ifdef _SVID_SOURCE
    LOG_DEBUG("_SVID_SOURCE defined\n");
#endif

#ifdef _ATFILE_SOURCE
    LOG_DEBUG("_ATFILE_SOURCE defined\n");
#endif

#ifdef _GNU_SOURCE
    LOG_DEBUG("_GNU_SOURCE defined\n");
#endif

#ifdef _REENTRANT
    LOG_DEBUG("_REENTRANT defined\n");
#endif

#ifdef _THREAD_SAFE
    LOG_DEBUG("_THREAD_SAFE defined\n");
#endif

#ifdef _FORTIFY_SOURCE
    LOG_DEBUG("_FORTIFY_SOURCE defined\n");
#endif
}

//============================================================================================================
// MAIN
//============================================================================================================

int main() {

    if (!setlocale(LC_CTYPE, "")) {
        fprintf(stderr, "Can't set the specified locale! "
                "Check LANG, LC_CTYPE, LC_ALL.\n");
        return 1;
    }

#ifndef NDEBUG
    init_allocators();
#endif

#ifdef ENABLE_LOGGING
#ifdef NDEBUG
    log_init(LOGLEVEL_WARN, "HWMonitor.log");
#else
    log_init(LOGLEVEL_DEBUG, "HWMonitor.log");
#endif
#endif


#ifndef NDEBUG
    check_style_defines();
    tests_run();
#endif

    pthread_mutex_init(&ldevices_mtx, NULL);
    pthread_mutex_init(&lnet_devs_mtx, NULL);
    pthread_mutex_init(&cpu_info_mtx, NULL);
    pthread_mutex_init(&mem_info_mtx, NULL);

    pthread_t blk_dev_thr;

    pthread_create(&blk_dev_thr, NULL, &start_blkdev_sample, NULL);
    pthread_setname_np(blk_dev_thr, "blkdev_sample");

    pthread_t net_dev_thr;
    pthread_create(&net_dev_thr, NULL, &start_net_dev_sample, NULL);
    pthread_setname_np(net_dev_thr, "netdev_sample");

    pthread_t cpu_dev_thr;
    pthread_create(&cpu_dev_thr, NULL, &start_cpu_dev_sample, NULL);
    pthread_setname_np(cpu_dev_thr, "cpudev_sample");

    pthread_t mem_info_thr;
    pthread_create(&mem_info_thr, NULL, &start_mem_info_sample, NULL);
    pthread_setname_np(mem_info_thr, "meminfo_sample");

    signal(SIGINT, &sig_handler);
    signal(SIGTERM, &sig_handler);
    signal(SIGUSR1, &sig_handler);

    ncurses_window();

    pthread_join(blk_dev_thr, NULL);
    pthread_join(net_dev_thr, NULL);
    pthread_join(cpu_dev_thr, NULL);
    pthread_join(mem_info_thr, NULL);

    pthread_mutex_destroy(&ldevices_mtx);
    pthread_mutex_destroy(&lnet_devs_mtx);
    pthread_mutex_destroy(&cpu_info_mtx);
    pthread_mutex_destroy(&mem_info_mtx);

#ifndef NDEBUG
    alloc_dump_summary();
    shutdown_allocators();
#endif

#ifdef ENABLE_LOGGING
    log_shutdown();
#endif

    return 0;
}

