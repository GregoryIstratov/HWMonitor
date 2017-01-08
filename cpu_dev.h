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
#include "string.h"

//============================================================================================================
// CPU DEVICE
//============================================================================================================

typedef struct cpu_dev {
    u64 user;
    u64 nice;
    u64 system;
    u64 idle;
    u64 iowait;
    u64 irc;
    u64 softirc;
    u64 steal;
    u64 guest;
    u64 guest_nice;
} cpu_dev_t;

void cpu_dev_release_cb(void* p);

void cpu_dev_get(cpu_dev_t** cpu_dev);

double cpu_dev_diff_usage(cpu_dev_t* a, cpu_dev_t* b);

typedef struct cpu_info {
    string* name;
    string* clock;
    u64 cores;
} cpu_info_t;

void cpu_info_release_cb(void* p);

void cpu_info_get(cpu_info_t** cpu_info);
