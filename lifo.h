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
// GENERIC LIFO
//============================================================================================================

typedef struct lifo_node {
    void* data;
    struct lifo_node* prev;

} lifo_node_t;

typedef struct lifo {
    lifo_node_t* head;
    u64 size;
    data_release_cb rel_cb;

} lifo_t;

void lifo_init(lifo_t** l, data_release_cb cb);

void lifo_node_init(lifo_node_t** node);

void lifo_push(lifo_t* l, void* s);

void* lifo_pop(lifo_t* l);

ret_t lifo_release(lifo_t* l, bool data_release);

#ifndef NDEBUG

void test_lifo(void);

#endif
