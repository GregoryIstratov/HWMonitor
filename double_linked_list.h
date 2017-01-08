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
// GENERIC DOUBLE LINKED LIST
//============================================================================================================

#include "globals.h"

typedef struct list_node {
    void* data;
    struct list_node* prev;
    struct list_node* next;

} list_node_t;

typedef struct list {
    list_node_t* head;
    list_node_t* tail;
    u64 size;
    data_release_cb rel_cb;

} list_t;

typedef struct list_iter {
    list_node_t* node;
} list_iter_t;

void list_iter_init(list_t* l, list_iter_t** it);

void list_iter_release(list_iter_t* it);

void* list_iter_next(list_iter_t* it);

void list_init(list_t** l, data_release_cb cb);

void list_node_init(list_node_t** node);

void list_push(list_t* l, void* s);

void* list_pop_head(list_t* l);

void* list_crop_tail(list_t* l);

ret_t list_release(list_t* l, bool release_data);

ret_t list_merge(list_t* __restrict a, list_t* __restrict b);

//TODO segfault
ret_t list_remove(list_t* l, const void* data);

typedef void(* list_traverse_cb)(list_node_t*);

ret_t list_traverse(list_t* l, bool forward, list_traverse_cb cb);

#ifndef NDEBUG

void test_list(void);

#endif
