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
#include "binary_tree.h"
#include "crc64.h"
#include "allocators.h"
#include "log.h"

ret_t bt_node_create(bt_node_t** bt, u64 hash, void* data) {
    *bt = zalloc(sizeof(bt_node_t));
    bt_node_t* b = *bt;
    b->hash_key = hash;

    b->data = data;

    return ST_OK;
}

ret_t bt_node_release(bt_node_t* bt, bool release_data) {

    if (bt != NULL) {

        bt_node_release(bt->left, release_data);
        bt_node_release(bt->right, release_data);

        if (release_data)
            bt->rel_cb(bt->data);

        SAFE_RELEASE(bt);

    }

    return ST_OK;
}

ret_t bt_node_set(bt_node_t** bt, bt_node_t* prev, u64 hash, void* data, data_release_cb rel_cb) {
    if (*bt == NULL) {
        *bt = zalloc(sizeof(bt_node_t));
        bt_node_t* b = *bt;
        b->hash_key = hash;
        b->prev = prev;
        b->rel_cb = rel_cb;

        b->data = data;

        return ST_OK;
    } else {
        bt_node_t* b = *bt;

        if (b->hash_key > hash)
            return bt_node_set(&b->right, b, hash, data, rel_cb);
        else if (b->hash_key < hash)
            return bt_node_set(&b->left, b, hash, data, rel_cb);
        else {

            if ((*bt)->rel_cb)
                (*bt)->rel_cb(b->data);

            b->data = data;

            return ST_OK;
        }

    }
}

ret_t bt_node_get(bt_node_t* bt, u64 hash, void** data) {
    if (bt == NULL)
        return ST_NOT_FOUND;

    if (bt->hash_key > hash)
        return bt_node_get(bt->right, hash, data);
    else if (bt->hash_key < hash)
        return bt_node_get(bt->left, hash, data);
    else {
        *data = bt->data;
        return ST_OK;
    }
}

ret_t bt_node_left(bt_node_t* bt, bt_node_t** left) {
    bt_node_t* cur = bt;

    while (cur && cur->left)
        cur = cur->left;

    *left = cur;

    return ST_OK;
}

void bt_node_traverse(bt_node_t* bt, bt_node_traverse_cb cb) {
    if (bt == NULL)
        return;

    bt_node_traverse(bt->left, cb);
    bt_node_traverse(bt->right, cb);

    cb(bt);
}

ret_t bt_init(binary_tree_t** bt, data_release_cb cb) {
    *bt = zalloc(sizeof(binary_tree_t));
    (*bt)->rel_cb = cb;

    return ST_OK;
}

ret_t bt_release(binary_tree_t* bt, bool release_data) {

    if (bt != NULL) {
        bt_node_release(bt->head, release_data);

        SAFE_RELEASE(bt);
    }

    return ST_OK;
}

#ifndef NDEBUG

str_int_t heap_str_int_decode(void* p) {
    str_int_t i;
    memcpy(&i, p, sizeof(str_int_t));

    return i;
}

ret_t bt_si_set(binary_tree_t* bt, const char* key, u64 i) {
    u64 hash = crc64s(key);
    str_int_t* data = zalloc(sizeof(str_int_t));
    data->str = key;
    data->i = i;
    return bt_node_set(&bt->head, NULL, hash, data, bt->rel_cb);
}

void bt_si_release(void* p) {
    str_int_t* data = (str_int_t*)p;
    zfree(data);
}

ret_t bt_si_get(binary_tree_t* bt, const char* key, str_int_t** i) {
    u64 hash = crc64s(key);
    void* p = NULL;
    ret_t ret = bt_node_get(bt->head, hash, &p);
    if (ret != ST_OK)
        return ret;

    *i = (str_int_t*)p;

    return ret;
}

void bt_si_traverse_cb(bt_node_t* node) {
    ret_t counter = 1;
    str_int_t i = heap_str_int_decode(node->data);

    LOG_DEBUG("[%d][%08lX] %s : 0x%lX", counter++, node->hash_key, i.str, i.i);
}

void bt_si_traverse(binary_tree_t* bt) {
    bt_node_traverse(bt->head, &bt_si_traverse_cb);
}

void test_hash_bt(void) {
    binary_tree_t* bt;
    str_int_t* node = NULL;
    bt_init(&bt, &bt_si_release);

    bt_si_set(bt, "1", 1);
    bt_si_set(bt, "2", 2);
    bt_si_set(bt, "3", 3);
    bt_si_set(bt, "4", 4);
    bt_si_set(bt, "5", 5);
    bt_si_set(bt, "6", 6);
    bt_si_set(bt, "7", 7);
    bt_si_set(bt, "8", 8);
    bt_si_set(bt, "9", 9);
    bt_si_set(bt, "10", 0xA);
    bt_si_set(bt, "11", 0xB);
    bt_si_set(bt, "12", 0xC);
    bt_si_set(bt, "13", 0xD);
    bt_si_set(bt, "14", 0xE);
    bt_si_set(bt, "15", 0xF);

    bt_si_get(bt, "1", &node);
    ASSERT(node->i == 0x1);
    bt_si_get(bt, "2", &node);
    ASSERT(node->i == 0x2);
    bt_si_get(bt, "3", &node);
    ASSERT(node->i == 0x3);
    bt_si_get(bt, "4", &node);
    ASSERT(node->i == 0x4);
    bt_si_get(bt, "5", &node);
    ASSERT(node->i == 0x5);
    bt_si_get(bt, "6", &node);
    ASSERT(node->i == 0x6);
    bt_si_get(bt, "7", &node);
    ASSERT(node->i == 0x7);
    bt_si_get(bt, "8", &node);
    ASSERT(node->i == 0x8);
    bt_si_get(bt, "9", &node);
    ASSERT(node->i == 0x9);
    bt_si_get(bt, "10", &node);
    ASSERT(node->i == 0xA);
    bt_si_get(bt, "11", &node);
    ASSERT(node->i == 0xB);
    bt_si_get(bt, "12", &node);
    ASSERT(node->i == 0xC);
    bt_si_get(bt, "13", &node);
    ASSERT(node->i == 0xD);
    bt_si_get(bt, "14", &node);
    ASSERT(node->i == 0xE);
    bt_si_get(bt, "15", &node);
    ASSERT(node->i == 0xF);

    bt_si_get(bt, "11", &node);
    node->i = 0xB;
    bt_si_get(bt, "12", &node);
    node->i = 0xC;
    bt_si_get(bt, "13", &node);
    node->i = 0xD;
    bt_si_get(bt, "14", &node);
    node->i = 0xE;
    bt_si_get(bt, "15", &node);
    node->i = 0xF;

    bt_si_get(bt, "11", &node);
    ASSERT(node->i == 0xB);
    bt_si_get(bt, "12", &node);
    ASSERT(node->i == 0xC);
    bt_si_get(bt, "13", &node);
    ASSERT(node->i == 0xD);
    bt_si_get(bt, "14", &node);
    ASSERT(node->i == 0xE);
    bt_si_get(bt, "15", &node);
    ASSERT(node->i == 0xF);

    bt_si_traverse(bt);
    bt_release(bt, true);
    bt_init(&bt, &bt_si_release);

    bt_si_set(bt, "1", 0);
    bt_si_set(bt, "2", 0);
    bt_si_set(bt, "3", 0);
    bt_si_set(bt, "4", 0);
    bt_si_set(bt, "5", 0);
    bt_si_set(bt, "6", 0);
    bt_si_set(bt, "7", 0);
    bt_si_set(bt, "8", 0);
    bt_si_set(bt, "9", 0);
    bt_si_set(bt, "10", 0x0);
    bt_si_set(bt, "11", 0x0);
    bt_si_set(bt, "12", 0x0);
    bt_si_set(bt, "13", 0x0);
    bt_si_set(bt, "14", 0x0);
    bt_si_set(bt, "15", 0x0);

    bt_si_get(bt, "1", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "2", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "3", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "4", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "5", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "6", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "7", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "8", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "9", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "10", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "11", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "12", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "13", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "14", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "15", &node);
    ASSERT(node->i == 0);

    bt_si_get(bt, "10", &node);
    node->i = 0xA;
    bt_si_get(bt, "11", &node);
    node->i = 0xB;
    bt_si_get(bt, "12", &node);
    node->i = 0xC;
    bt_si_get(bt, "13", &node);
    node->i = 0xD;
    bt_si_get(bt, "14", &node);
    node->i = 0xE;
    bt_si_get(bt, "15", &node);
    node->i = 0xF;

    bt_si_get(bt, "10", &node);
    ASSERT(node->i == 0xA);
    bt_si_get(bt, "11", &node);
    ASSERT(node->i == 0xB);
    bt_si_get(bt, "12", &node);
    ASSERT(node->i == 0xC);
    bt_si_get(bt, "13", &node);
    ASSERT(node->i == 0xD);
    bt_si_get(bt, "14", &node);
    ASSERT(node->i == 0xE);
    bt_si_get(bt, "15", &node);
    ASSERT(node->i == 0xF);

    bt_si_traverse(bt);

    bt_release(bt, true);
    bt = NULL;
}

#endif
