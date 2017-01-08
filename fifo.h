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
// GENERIC FIFO
//============================================================================================================

typedef struct fifo_node {
    void* data;
    struct fifo_node* next;

} fifo_node_t;

typedef struct fifo {
    fifo_node_t* head;
    fifo_node_t* top;
    u64 size;
    data_release_cb rel_cb;

} fifo_t;

void fifo_init(fifo_t** l, data_release_cb cb);

void fifo_node_init(fifo_node_t** node);

void fifo_push(fifo_t* l, void* s);

void* fifo_pop(fifo_t* l);

ret_t fifo_release(fifo_t* l, bool data_release);

#ifndef NDEBUG

void test_fifo(void);

#endif

