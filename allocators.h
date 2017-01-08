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
// ALLOCATORS
//============================================================================================================
#include <stdlib.h>

#include "globals.h"


void alloc_dump_summary(void);

ret_t init_allocators(void);

void shutdown_allocators(void);

#ifdef NDEBUG

void* zalloc(u64 size);

#define zfree(p) free(p)
#define zrealloc(p, size) realloc((p), (size))

#else

void* _zalloc(u64 size, u64 line, const char* fun);

void* _zrealloc(void* p, u64 size, u64 line, const char* fun);

void _zfree(void* p, u64 line, const char* fun);

#define zalloc(p) _zalloc((p), __LINE__, __func__)
#define zfree(p) _zfree((p), __LINE__, __func__)
#define zrealloc(p, size) _zrealloc((p), (size), __LINE__, __func__)

#endif

void* mmove(void* __restrict dst, const void* __restrict src, u64 size);

void* mcopy(void* __restrict dst, const void* __restrict src, u64 size);

