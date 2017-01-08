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
#include "vector.h"
#include "dynamic_allocator.h"
#include "allocators.h"

ret_t vector_init(vector_t** vec, u64 elem_size) {
    *vec = zalloc(sizeof(vector_t));
    da_init_n(&(*vec)->alloc, elem_size * 10);
    (*vec)->elem_size = elem_size;

    return ST_OK;
}

ret_t vector_release(vector_t* vec) {
    da_release(vec->alloc);
    zfree(vec);

    return ST_OK;
}

ret_t vector_add(vector_t* vec, const void* elem) {
    da_append(vec->alloc, (const char*)elem, vec->elem_size);
    vec->size++;

    return ST_OK;
}

ret_t vector_get(vector_t* vec, u64 idx, void** elem) {
    if (idx >= vec->size)
        return ST_OUT_OF_RANGE;

    *elem = (void*)&(vec->alloc->ptr[vec->elem_size * idx]);

    return ST_OK;
}

ret_t vector_set(vector_t* vec, u64 idx, void* elem) {
    if (idx >= vec->size)
        return ST_OUT_OF_RANGE;

    void* el = (void*)&(vec->alloc->ptr[vec->elem_size * idx]);

    memcpy(el, elem, vec->elem_size);

    return ST_OK;

}

typedef void(* vector_foreach_cb)(u64, u64, void*, void*);

void vector_foreach(vector_t* vec, void* ctx, vector_foreach_cb cb) {
    u64 n = vec->size;
    for (u64 i = 0; i < n; ++i) {
        void* v = (void*)&(vec->alloc->ptr[vec->elem_size * i]);
        cb(i, vec->elem_size, ctx, v);
    }
}
