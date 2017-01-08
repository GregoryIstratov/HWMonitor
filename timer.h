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

//============================================================================================================
// TIMERS AND SLEEP
//============================================================================================================
#include <time.h>
#include "globals.h"

#define NANOSEC_IN_SEC      1000000000.0
#define NANOSEC_IN_MILLISEC 1000000.0

struct timespec timer_start(void);

double timer_end_ms(struct timespec start_time);

void nsleep(u64 nanoseconds);

static inline void nsleepd(double seconds) {
    nsleep((u64)(seconds * NANOSEC_IN_SEC));
}
