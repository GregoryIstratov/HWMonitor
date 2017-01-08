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

#include <stdlib.h>
#include <string.h>
#include "allocators.h"
#include "concurrent_hashtable.h"
#include "crc64.h"
#include "log.h"

static hashtable_t* g_alloc_ht = NULL;

typedef struct alloc_info {
    u64 ptr;
    u64 size;
    u64 allocated;
} alloc_info_t;

static void common_release_cb(void* p) {
    free(p);
}

static inline u64 crc64i(u64* i) {
    return crc64(0, (uint8_t*)i, 8);
}

static inline u64 ht_hasher_u64(void* i) {
    return crc64i((u64*)i);
}

static inline ret_t alloc_set_info(void* p, u64 size, u64 allocated) {
    alloc_info_t* info = (alloc_info_t*)calloc(1, sizeof(alloc_info_t));
    info->allocated = allocated;
    info->ptr = PTR_TO_U64(p);
    info->size = size;

    u64* key = (u64*)calloc(1, sizeof(u64));
    *key = PTR_TO_U64(p);

    return ht_set(g_alloc_ht, key, info);
}

static inline ret_t alloc_del_info(void* p) {
    u64 pi = PTR_TO_U64(p);

    return ht_del(g_alloc_ht, &pi);
}

static void alloc_summary_cb(u64 hash, void* key, void* value, void* ctx) {
    u64* allocated = (u64*)ctx;

    *allocated += ((alloc_info_t*)value)->allocated;
}

void alloc_dump_summary() {
    u64 allocated = 0;

    ht_foreach(g_alloc_ht, &alloc_summary_cb, &allocated);

    LOG_DEBUG("Current allocated memory %lu bytes in %lu elements", allocated, ht_size(g_alloc_ht));

    ht_hist_dump_csv(g_alloc_ht, "alloc_ht_hist.csv");
}

ret_t init_allocators() {
    ht_init(&g_alloc_ht, 256, &ht_hasher_u64, &common_release_cb, &common_release_cb);

    return ST_OK;
}

void shutdown_allocators(void)
{
    ht_destroy(g_alloc_ht);
    g_alloc_ht = NULL;
}

#ifdef NDEBUG

void* zalloc(u64 size) {
    void* v = malloc(size);
    if (v == NULL) {
        LOG_ERROR("malloc returns null pointer [size=%lu]. Trying again...", size);
        return NULL;
    }

    memset(v, 0, size);

    return v;

}

#else

void* _zalloc(u64 size, u64 line, const char* fun) {
    void* v = malloc(size);
    if (v == NULL) {
        LOG_ERROR("malloc returns null pointer [size=%lu]. Trying again...", size);
        return NULL;
    }

    memset(v, 0, size);

    if (alloc_set_info(v, size, size) != ST_OK) {
        _log(__FILE__, line, fun, LOG_ERROR, "Failed to set alloc info");
    } else {
        _log(__FILE__, line, fun, LOG_TRACE, "Alloc info set [0x%lX]", PTR_TO_U64(v));
    }

    return v;

}

void* _zrealloc(void* p, u64 size, u64 line, const char* fun) {
    void* v = realloc(p, size);

    if (p != v) {
        if (alloc_del_info(p) == ST_OK) {
            _log(__FILE__, line, fun, LOG_TRACE, "[realloc] info free [0x%lX]", p);
        }
    }

    if (v) {
        if (alloc_set_info(v, size, size) != ST_OK) {
            _log(__FILE__, line, fun, LOG_ERROR, "[zrealloc] Failed to set alloc info");
        } else {
            _log(__FILE__, line, fun, LOG_TRACE, "Alloc info set [0x%lX]", PTR_TO_U64(v));
        }
    }

    return v;

}

void _zfree(void* p, u64 line, const char* fun) {
    if (!p)
        return;

    if (alloc_del_info(p) != ST_OK) {
        _log(__FILE__, line, fun, LOG_ERROR, "alloc info not found [0x%lX]", p);
    } else {
        _log(__FILE__, line, fun, LOG_TRACE, "alloc info free [0x%lX]", p);
    }

    free(p);
}

#endif

void* mmove(void* __restrict dst, const void* __restrict src, u64 size) {
    return memmove(dst, src, size);
}

void* mcopy(void* __restrict dst, const void* __restrict src, u64 size) {
    return memcpy(dst, src, size);
}

