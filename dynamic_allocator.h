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
// DYNAMIC ALLOCATOR
//============================================================================================================

#include "globals.h"

typedef struct dynamic_allocator {
    char* ptr;
    u64 size;
    u64 used;
    u64 mul;

} dynamic_allocator_t;

ret_t da_realloc(dynamic_allocator_t* a, u64 size);

ret_t da_init_n(dynamic_allocator_t** a, u64 size);

ret_t da_init(dynamic_allocator_t** a);

ret_t da_release(dynamic_allocator_t* a);

ret_t da_fit(dynamic_allocator_t* a);

ret_t da_crop_tail(dynamic_allocator_t* a, u64 n);

ret_t da_pop_head(dynamic_allocator_t* a, u64 n);

ret_t da_check_size(dynamic_allocator_t* a, u64 new_size);

ret_t da_append(dynamic_allocator_t* a, const char* data, u64 size);

ret_t da_sub(dynamic_allocator_t* a, u64 begin, u64 end, dynamic_allocator_t** b);

ret_t da_dub(dynamic_allocator_t* a, dynamic_allocator_t** b);

ret_t da_merge(dynamic_allocator_t* a, dynamic_allocator_t** b);

ret_t da_concat(dynamic_allocator_t* __restrict a, dynamic_allocator_t* __restrict b);

ret_t da_remove(dynamic_allocator_t* a, u64 begin, u64 end);

ret_t da_compare(dynamic_allocator_t* a, dynamic_allocator_t* b);

ret_t da_comparez(dynamic_allocator_t* a, const char* b);

void test_da(void);
