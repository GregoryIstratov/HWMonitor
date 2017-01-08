/*************************************************************************************************************
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
            the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
            but WITHOUT ANY WARRANTY; without even the implied warranty of
                            MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************************************/
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
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "concurrent_hashtable.h"
#include "log.h"

ret_t ht_init(hashtable_t** ht,
                     u64 table_size,
                     ht_key_hasher hasher,
                     ht_data_releaser key_releaser,
                     ht_data_releaser value_releaser) {
    *ht = calloc(sizeof(hashtable_t), 1);
    hashtable_t* pht = *ht;
    pht->table_size = table_size;
    pht->hasher = hasher;
    pht->key_releaser = key_releaser;
    pht->value_releaser = value_releaser;
    pht->table = (ht_item_t**)calloc(table_size, sizeof(ht_item_t*));
    pht->bin_locks = (pthread_spinlock_t*)calloc(table_size, sizeof(pthread_spinlock_t));
    atomic_store_explicit(&pht->size, 0, memory_order_relaxed);
    pht->bin_size = (u64*)calloc(table_size, sizeof(u64));

    for (u64 i = 0; i < table_size; ++i) {
        pthread_spin_init(&pht->bin_locks[i], 0);
    }

    return ST_OK;
}

void ht_destroy_item(hashtable_t* ht, ht_item_t* item) {
    if (item) {
        ht->key_releaser((item)->key);
        ht->value_releaser((item)->value);
        free(item);
    }
}

void ht_destroy_items_line(hashtable_t* ht, ht_item_t* start_item) {
    ht_item_t* next = start_item;
    ht_item_t* tmp = NULL;
    while (next) {
        tmp = next;
        next = next->next;

        ht_destroy_item(ht, tmp);
    }
}

void ht_destroy(hashtable_t* ht) {
    for (u64 i = 0; i < ht->table_size; ++i) {
        ht_destroy_items_line(ht, ht->table[i]);
        pthread_spin_destroy(&ht->bin_locks[i]);
    }

    free((void*)ht->bin_size);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
    free((void*)ht->bin_locks);
#pragma clang diagnostic pop
    free((void*)ht->table);
    free(ht);
}

ret_t ht_set(hashtable_t* ht, void* key, void* value) {
    u64 hash = ht->hasher(key);
    u64 bin = hash % ht->table_size;

    pthread_spin_lock(&ht->bin_locks[bin]);

    ht_item_t* item = ht->table[bin];
    ht_item_t* prev = NULL;
    while (item) {
        if (item->hash == hash)
            break;

        prev = item;
        item = item->next;
    }

    if (item && item->hash == hash) {
        ht->value_releaser(item->value);
        item->value = value;
        free(key);
    } else {
        ht_item_t* new_item = NULL;
        if ((new_item = calloc(1, sizeof(ht_item_t))) == NULL) {
            pthread_spin_unlock(&ht->bin_locks[bin]);

            LOG_ERROR("can't alloc");
            return ST_ERR;
        }

        new_item->hash = hash;
        new_item->key = key;
        new_item->value = value;

        if (prev)
            prev->next = new_item;
        else
            ht->table[bin] = new_item;

        ++ht->bin_size[bin];
        atomic_fetch_add(&ht->size, 1);
    }

    pthread_spin_unlock(&ht->bin_locks[bin]);

    return ST_OK;
}

ret_t ht_get(hashtable_t* ht, void* key, void** value) {
    u64 hash = ht->hasher(key);
    u64 bin = hash % ht->table_size;

    pthread_spin_lock(&ht->bin_locks[bin]);
    ht_item_t* item = ht->table[bin];

    while (item) {
        if (item->hash == hash) {
            *value = item->value;
            pthread_spin_unlock(&ht->bin_locks[bin]);

            return ST_OK;
        }

        item = item->next;
    }

    pthread_spin_unlock(&ht->bin_locks[bin]);

    return ST_NOT_FOUND;
}

ret_t ht_del(hashtable_t* ht, void* key) {
    u64 hash = ht->hasher(key);
    u64 bin = hash % ht->table_size;

    pthread_spin_lock(&ht->bin_locks[bin]);

    ht_item_t* item = ht->table[bin];
    ht_item_t* prev = NULL;
    while (item) {
        if ((item)->hash == hash) {

            if (prev && (item)->next) {
                prev->next = (item)->next;
                ht_destroy_item(ht, item);
            } else if (prev) {
                prev->next = NULL;
                ht_destroy_item(ht, item);
            } else if (item->next) {
                ASSERT_EQ(item, ht->table[bin]);

                ht->table[bin] = item->next;

                ht_destroy_item(ht, item);
            } else {
                ASSERT_EQ(item, ht->table[bin]);
                ht_destroy_item(ht, item);
                ht->table[bin] = NULL;
            }

            --ht->bin_size[bin];
            atomic_fetch_sub(&ht->size, 1);

            pthread_spin_unlock(&ht->bin_locks[bin]);
            return ST_OK;
        }

        prev = item;
        item = (item)->next;
    }

    pthread_spin_unlock(&ht->bin_locks[bin]);
    return ST_NOT_FOUND;
}

u64 ht_size(hashtable_t* ht) {
    return atomic_load(&ht->size);
}

u64 ht_table_size(hashtable_t* ht) {
    return ht->table_size;
}

u64 ht_bin_size(hashtable_t* ht, u64 bin) {
    if (bin >= ht->table_size) {
        LOG_ERROR("out of range");
        return 0;
    }

    u64 size = 0;
    pthread_spin_lock(&ht->bin_locks[bin]);

    size = ht->bin_size[bin];

    pthread_spin_unlock(&ht->bin_locks[bin]);

    return size;
}

ret_t ht_hist_dump_csv(hashtable_t* ht, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        char* err = strerror(errno);
        LOG_ERROR("can't open the file %s, error=", filename, err);
        free(err);
        return ST_ERR;
    }

    char buff[4096];
    memset(buff, 0, 4096);

    char* s = buff;

    for (u64 i = 0; i < ht->table_size; ++i) {
        int n = sprintf(s, "%lu,%lu\n", i, ht_bin_size(ht, i));
        s += n;
    }

    fwrite(buff, sizeof(char), sizeof(buff), f);

    fclose(f);

    return ST_OK;
}


ret_t ht_foreach(hashtable_t* ht, ht_foreach_cb cb, void* ctx) {
    for (u64 i = 0; i < ht->table_size; ++i) {
        pthread_spin_lock(&ht->bin_locks[i]);
        ht_item_t* next = ht->table[i];
        while (next) {
            cb(next->hash, next->key, next->value, ctx);
            next = next->next;
        }
        pthread_spin_unlock(&ht->bin_locks[i]);
    }

    return ST_OK;
}
