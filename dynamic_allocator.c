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

#include <string.h>
#include <sys/param.h>
#include "dynamic_allocator.h"
#include "log.h"
#include "allocators.h"

#define DA_TRACE(a) (LOG_TRACE("[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu", \
a, a->ptr, a->size, a->used, a->mul))


ret_t da_realloc(dynamic_allocator_t* a, u64 size) {
    if (a == NULL) {
        LOG_WARN("Empty dynamic_allocator::ptr");
        DA_TRACE(a);

        return da_init(&a);
    }

    if (a->size == size) {
        return ST_OK;
    } else if (size < a->size || size == 0) {
        u64 ds = a->size - size;
        memset(a->ptr + size, 0, ds);
        a->ptr = zrealloc(a->ptr, size);
        a->size = size;
        a->used = size;
        a->mul = 1;
    } else if (size > a->size) {
        u64 ds = size - a->size;
        a->ptr = zrealloc(a->ptr, size);
        memset(a->ptr + a->size, 0, ds);
        a->size = size;
    }

    return ST_OK;
}

ret_t da_init_n(dynamic_allocator_t** a, u64 size) {
    *a = zalloc(sizeof(dynamic_allocator_t));

    (*a)->ptr = zalloc(size);
    (*a)->size = size;
    (*a)->mul = 1;

    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              (*a), (*a)->ptr, (*a)->size, (*a)->used, (*a)->mul);

    return ST_OK;
}

ret_t da_init(dynamic_allocator_t** a) {

    return da_init_n(a, STRING_INIT_BUFFER);
}

ret_t da_release(dynamic_allocator_t* a) {
    if (!a)
        return ST_EMPTY;

    if (a) {
        LOG_TRACE("a[0x%08lX] size=%lu used=%lu mul=%lu",
                  a->ptr, a->size, a->used, a->mul);

        SAFE_RELEASE(a->ptr);
        SAFE_RELEASE(a);
    }

    return ST_OK;;
}

ret_t da_fit(dynamic_allocator_t* a) {

    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    da_realloc(a, a->used);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return ST_OK;
}

ret_t da_crop_tail(dynamic_allocator_t* a, u64 n) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
              a, a->ptr, a->size, a->used, a->mul, n);

    if (n > a->size) {
        LOG_WARN("a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
                 a, a->ptr, a->size, a->used, a->mul, n);

        return ST_OUT_OF_RANGE;
    }

    u64 nsize = a->used - n;
    char* newbuff = zalloc(nsize);
    memcpy(newbuff, a->ptr + n, nsize);
    zfree(a->ptr);
    a->ptr = newbuff;
    a->size = nsize;
    a->used = a->size;

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return ST_OK;
}

ret_t da_pop_head(dynamic_allocator_t* a, u64 n) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; n=%lu",
              a, a->ptr, a->size, a->used, a->mul, n);

    da_fit(a);
    if (n > a->size) {
        LOG_WARN("a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
                 a, a->ptr, a->size, a->used, a->mul, n);

        return ST_OUT_OF_RANGE;
    }

    da_realloc(a, a->size - n);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return ST_OK;
}

ret_t da_check_size(dynamic_allocator_t* a, u64 new_size) {
    if (a->size < a->used + new_size)
        da_realloc(a, a->used + new_size);

    return ST_OK;
}

ret_t da_append(dynamic_allocator_t* a, const char* data, u64 size) {

    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    LOG_TRACE("input data=0x%08lX size=%lu", data, size);

    da_check_size(a, size);

    mcopy(a->ptr + a->used, data, size);
    a->used += size;

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return ST_OK;
}

ret_t da_sub(dynamic_allocator_t* a, u64 begin, u64 end, dynamic_allocator_t** b) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; input begin=%lu end=%lu",
              a, a->ptr, a->size, a->used, a->mul, begin, end);

    u64 ssize = end - begin;
    if (ssize > a->used) {

        LOG_WARN("a=[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; begin=%lu and end=%lu out of range",
                 a, a->ptr, a->size, a->used, a->mul, begin, end);

        return ST_OUT_OF_RANGE;
    }

    da_init_n(b, ssize);
    da_append(*b, a->ptr + begin, ssize);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    LOG_TRACE("e b[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              (*b), (*b)->ptr, (*b)->size, (*b)->used, (*b)->mul);

    return ST_OK;

}

ret_t da_dub(dynamic_allocator_t* a, dynamic_allocator_t** b) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    u64 b_size = a->used;
    da_init_n(b, b_size);
    da_append(*b, a->ptr, b_size);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              (*b), (*b)->ptr, (*b)->size, (*b)->used, (*b)->mul);

    return ST_OK;
}

ret_t da_merge(dynamic_allocator_t* a, dynamic_allocator_t** b) {
    DA_TRACE(a);
    DA_TRACE((*b));

    da_fit(a);
    da_fit(*b);

    u64 nb_size = a->size + (*b)->size;

    da_realloc(a, nb_size);

    mcopy(a->ptr + a->used, (*b)->ptr, (*b)->size);

    a->used += (*b)->size;

    da_release(*b);
    *b = NULL;

    DA_TRACE(a);
    ASSERT(*b == NULL);

    return ST_OK;
}

ret_t da_concat(dynamic_allocator_t* __restrict a, dynamic_allocator_t* __restrict b) {
    DA_TRACE(a);
    DA_TRACE(b);

    da_fit(a);
    da_fit(b);

    u64 nb_size = a->size + b->size;

    da_realloc(a, nb_size);

    mcopy(a->ptr + a->used, b->ptr, b->size);

    a->used += b->size;

    DA_TRACE(a);
    DA_TRACE(b);

    return ST_OK;
}

ret_t da_remove(dynamic_allocator_t* a, u64 begin, u64 end) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; begin=%lu end=%lu",
              a, a->ptr, a->size, a->used, a->mul, begin, end);

    u64 dsize = end - begin;
    if (dsize > a->used) {
        LOG_WARN("begin(%lu) + end(%lu) >= total used(%lu) bytes", begin, end, a->used);
        return ST_SIZE_EXCEED;
    }

    dynamic_allocator_t* b = NULL;

    da_sub(a, end, a->used, &b);

    da_realloc(a, begin);

    da_merge(a, &b);

    ASSERT(b == NULL);
    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return ST_OK;

}

ret_t da_compare(dynamic_allocator_t* a, dynamic_allocator_t* b) {
    if (a->used < b->used)
        return ST_NOT_FOUND;
    else if (a->used > b->used)
        return ST_NOT_FOUND;
    else if (memcmp(a->ptr, b->ptr, MIN(a->used, b->used)) == 0)
        return ST_OK;

    return ST_NOT_FOUND;
}

ret_t da_comparez(dynamic_allocator_t* a, const char* b) {
    u64 ssize = strlen(b);
    return memcmp(a->ptr, b, MIN(a->used, ssize)) == 0 ? ST_OK : ST_NOT_FOUND;
}

void test_da() {
    dynamic_allocator_t* da = NULL;
    dynamic_allocator_t* da2 = NULL;
    dynamic_allocator_t* db = NULL;
    dynamic_allocator_t* dc = NULL;
    dynamic_allocator_t* dd = NULL;
    dynamic_allocator_t* de = NULL;
    dynamic_allocator_t* df = NULL;

    CHECK_RETURN(da_init(&da));

    // 0    H
    // 1    e
    // 2    l
    // 3    l
    // 4    o
    // 5    ,
    // 6
    // 7    S
    // 8    w
    // 9    e
    // 10   e
    // 11   t
    // 12
    // 13   M
    // 14   a
    // 15   r
    // 16   i
    // 17   a
    // 18   !
    // 19   EOF

#define STR_TO_PZ(str) str, strlen(str)

//    char const *str1 = "Hello, Sweet Maria!";
    CHECK_RETURN(da_append(da, STR_TO_PZ("Hello, ")));
    CHECK_RETURN(da_append(da, STR_TO_PZ("Sweet Maria!")));

#undef STR_TO_PZ

    CHECK_RETURN(da_dub(da, &da2));
    CHECK_RETURN(da_remove(da2, 0, 7));

    CHECK_RETURN(da_comparez(da2, "Sweet Maria!"));

    CHECK_RETURN(da_sub(da, 13, 19, &db));

    CHECK_RETURN(da_comparez(db, "Maria!"));

    CHECK_RETURN(da_compare(db, db));

    CHECK_RETURN(da_init(&de));
    CHECK_RETURN(da_append(de, " My ", 4));

    CHECK_RETURN(da_comparez(de, " My "));

    CHECK_RETURN(da_sub(da, 7, 12, &df));

    CHECK_RETURN(da_comparez(df, "Sweet"));

    CHECK_RETURN(da_append(df, "!", 1));

    CHECK_RETURN(da_remove(da, 7, 13));

    CHECK_RETURN(da_comparez(da, "Hello, Maria!"));

    CHECK_RETURN(da_merge(de, &df));

    CHECK_RETURN(da_concat(da, de));

    da_release(da);
    da = NULL;

    da_release(da2);
    da2 = NULL;

    da_release(db);
    db = NULL;

    da_release(dc);
    dc = NULL;

    da_release(dd);
    dd = NULL;

    da_release(de);
    de = NULL;

    da_release(df);
    df = NULL;
}

