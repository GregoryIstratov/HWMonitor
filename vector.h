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
#include "dynamic_allocator.h"

//============================================================================================================
// GENERIC VECTOR
//============================================================================================================
typedef struct vector {
    dynamic_allocator_t* alloc;
    u64 size;
    u64 elem_size;
} vector_t;

ret_t vector_init(vector_t** vec, u64 elem_size);

ret_t vector_release(vector_t* vec);

static inline u64 vector_size(vector_t* vec) { return vec->size; }

ret_t vector_add(vector_t* vec, const void* elem);

ret_t vector_get(vector_t* vec, u64 idx, void** elem);

ret_t vector_set(vector_t* vec, u64 idx, void* elem);

typedef void(* vector_foreach_cb)(u64, u64, void*, void*);

void vector_foreach(vector_t* vec, void* ctx, vector_foreach_cb cb);
