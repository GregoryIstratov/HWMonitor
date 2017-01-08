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
#include <pthread.h>

#include "globals.h"



//============================================================================================================
// CONCURRENT HASH TABLE
//============================================================================================================
typedef u64(* ht_key_hasher)(void* key);

typedef void(* ht_data_releaser)(void* key);

typedef struct _ht_item_t {
    void* key;
    void* value;
    u64 hash;
    struct _ht_item_t* next;
} ht_item_t;

typedef struct _hashtable_t {
    u64 table_size;
    atomic_u64 size;
    u64* bin_size;
    ht_item_t** table;
    pthread_spinlock_t* bin_locks;
    ht_key_hasher hasher;
    ht_data_releaser key_releaser;
    ht_data_releaser value_releaser;
} hashtable_t;

ret_t ht_init(hashtable_t** ht,
                     u64 table_size,
                     ht_key_hasher hasher,
                     ht_data_releaser key_releaser,
                     ht_data_releaser value_releaser);

void ht_destroy_item(hashtable_t* ht, ht_item_t* item);

void ht_destroy_items_line(hashtable_t* ht, ht_item_t* start_item);

void ht_destroy(hashtable_t* ht);

ret_t ht_set(hashtable_t* ht, void* key, void* value);

ret_t ht_get(hashtable_t* ht, void* key, void** value);

ret_t ht_del(hashtable_t* ht, void* key);

u64 ht_size(hashtable_t* ht);

u64 ht_table_size(hashtable_t* ht);

u64 ht_bin_size(hashtable_t* ht, u64 bin);

ret_t ht_hist_dump_csv(hashtable_t* ht, const char* filename);

typedef void(* ht_foreach_cb)(u64, void*, void*, void*);

ret_t ht_foreach(hashtable_t* ht, ht_foreach_cb cb, void* ctx);
