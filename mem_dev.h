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
// MEMORY
//============================================================================================================

typedef struct mem_info {

    //Total usable memory
    u64 mem_total;

    //The amount of physical memory not used by the system
    u64 mem_free;

    //An estimate of how much memory is available for starting new applications, without swapping.
    u64 mem_avail;

    //Memory in the pagecache (Diskcache and Shared Memory)
    u64 cached;

    //Memory that is present within main memory, but also in the swapfile.
    //(If memory is needed this area does not need to be swapped out AGAIN because it is already
    // in the swapfile. This saves I/O and increases performance if machine runs short on memory.)
    u64 swap_cached;

    u64 swap_total;
    u64 swap_free;
} mem_info_t;

void mem_info_release_cb(void* p);

void mem_info_get(mem_info_t** mem_info);
