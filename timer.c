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

#include <errno.h>
#include "timer.h"

struct timespec timer_start() {
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    return start_time;
}

double timer_end_ms(struct timespec start_time) {
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    time_t ds = end_time.tv_sec - start_time.tv_sec;
    double dns = ds * NANOSEC_IN_SEC + (end_time.tv_nsec - start_time.tv_nsec);

    return dns / NANOSEC_IN_MILLISEC;
}

void nsleep(u64 nanoseconds) {
    struct timespec req;

    req.tv_sec = (time_t)(nanoseconds / (u64)NANOSEC_IN_SEC);
    req.tv_nsec = nanoseconds % (u64)NANOSEC_IN_SEC;

    struct timespec rem = {0, 0};

    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem;
    }

}
