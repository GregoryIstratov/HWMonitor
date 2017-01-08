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

#include <memory.h>
#include "cpu_dev.h"
#include "utils.h"
#include "allocators.h"

void cpu_dev_release_cb(void* p) {
    zfree(p);
}

void cpu_dev_get(cpu_dev_t** cpu_dev) {
    *cpu_dev = zalloc(sizeof(cpu_dev_t));
    cpu_dev_t* cpu = *cpu_dev;
    string* stat_s = NULL;
    string_init(&stat_s);

    file_read_line("/proc/stat", stat_s);
    string_strip(stat_s);

    list_t* cpu_stats = NULL;
    string_split(stat_s, ' ', &cpu_stats);
    string_release(stat_s);

    list_iter_t* stat_it = NULL;
    list_iter_init(cpu_stats, &stat_it);
    list_iter_next(stat_it);

    string* stat = NULL;
    u64 istats[10];
    u64 idx = 0;
    while ((stat = list_iter_next(stat_it))) {
        string_to_u64(stat, &istats[idx++]);
    }

    cpu->user = istats[0];
    cpu->nice = istats[1];
    cpu->system = istats[2];
    cpu->idle = istats[3];
    cpu->iowait = istats[4];
    cpu->irc = istats[5];
    cpu->softirc = istats[6];
    cpu->steal = istats[7];
    cpu->guest = istats[8];
    cpu->guest_nice = istats[9];

    list_iter_release(stat_it);
    list_release(cpu_stats, true);
}

double cpu_dev_diff_usage(cpu_dev_t* a, cpu_dev_t* b) {

    u64 prev_idle = a->idle + a->iowait;
    u64 idle = b->idle + b->iowait;

    u64 prev_non_idle = a->user + a->nice + a->system + a->irc + a->softirc + a->steal;
    u64 non_idle = b->user + b->nice + b->system + b->irc + b->softirc + b->steal;

    u64 prev_total = prev_idle + prev_non_idle;
    u64 total = idle + non_idle;

    u64 totald = total - prev_total;
    u64 idled = idle - prev_idle;

    double usage = (double)(totald - idled) / (double)totald;

    return usage;
}

void cpu_info_release_cb(void* p) {
    if (!p)
        return;

    cpu_info_t* cpui = (cpu_info_t*)p;

    if (cpui->name)
        string_release(cpui->name);
    if (cpui->clock)
        string_release(cpui->clock);

    zfree(cpui);
}

void cpu_info_get(cpu_info_t** cpu_info) {
    *cpu_info = zalloc(sizeof(cpu_info_t));
    cpu_info_t* cpu = *cpu_info;
    string* info_s = NULL;
    string_init(&info_s);

    file_read_all_buffered_s("/proc/cpuinfo", info_s);

    list_t* lines = NULL;
    string_split(info_s, '\n', &lines);

    list_iter_t* lines_it = NULL;
    list_iter_init(lines, &lines_it);

    string* line;
    u64 n_cpu = 0;
    while ((line = list_iter_next(lines_it))) {
        if (string_starts_with(line, "model name") == ST_OK) {
            if (n_cpu > 0)
                continue;
            string_crop_tail(line, strlen("model name\t:"));
            string_strip(line);
            string_dub(line, &cpu->name);
        }

        if (string_starts_with(line, "cpu MHz") == ST_OK) {
            if (n_cpu++ > 0)
                continue;
            string_crop_tail(line, strlen("cpu MHz\t: "));
            string_strip(line);
            string_dub(line, &cpu->clock);
        }

    }

    cpu->cores = n_cpu;

    list_iter_release(lines_it);
    list_release(lines, true);
    string_release(info_s);
}
