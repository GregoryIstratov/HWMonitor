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
// HASH BINARY TREE
//============================================================================================================

typedef struct bt_node {
    u64 hash_key;
    void* data;
    struct bt_node* prev;
    struct bt_node* left;
    struct bt_node* right;
    data_release_cb rel_cb;

} bt_node_t;

ret_t bt_node_create(bt_node_t** bt, u64 hash, void* data);

ret_t bt_node_release(bt_node_t* bt, bool release_data);

ret_t bt_node_set(bt_node_t** bt, bt_node_t* prev, u64 hash, void* data, data_release_cb rel_cb);

ret_t bt_node_get(bt_node_t* bt, u64 hash, void** data);

ret_t bt_node_left(bt_node_t* bt, bt_node_t** left);

typedef void(* bt_node_traverse_cb)(bt_node_t*);

void bt_node_traverse(bt_node_t* bt, bt_node_traverse_cb cb);
// froentends

typedef struct binary_tree {
    bt_node_t* head;
    data_release_cb rel_cb;
} binary_tree_t;

ret_t bt_init(binary_tree_t** bt, data_release_cb cb);

ret_t bt_release(binary_tree_t* bt, bool release_data);

#ifndef NDEBUG

//simple char*:int froentend

typedef struct str_int {
    const char* str;
    u64 i;
} str_int_t;

str_int_t heap_str_int_decode(void* p);

ret_t bt_si_set(binary_tree_t* bt, const char* key, u64 i);

void bt_si_release(void* p);

ret_t bt_si_get(binary_tree_t* bt, const char* key, str_int_t** i);

void bt_si_traverse_cb(bt_node_t* node);

void bt_si_traverse(binary_tree_t* bt);

void test_hash_bt(void);
#endif

