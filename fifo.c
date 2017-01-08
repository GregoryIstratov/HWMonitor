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
#include "fifo.h"
#include "log.h"
#include "allocators.h"

void fifo_init(fifo_t** l, data_release_cb cb) {
    *l = zalloc(sizeof(fifo_t));
    (*l)->rel_cb = cb;
}

void fifo_node_init(fifo_node_t** node) {
    *node = zalloc(sizeof(fifo_node_t));
}

void fifo_push(fifo_t* l, void* s) {

    if (!l->head) {
        fifo_node_init(&l->head);

        fifo_node_t* node = l->head;

        node->data = s;

        l->top = l->head;
    } else {
        fifo_node_t* node = NULL;
        fifo_node_init(&node);
        node->data = s;
        l->top->next = node;

        l->top = node;

    }

    ++l->size;
}

void* fifo_pop(fifo_t* l) {
    if (!l->head)
        return NULL;

    fifo_node_t* tmp = l->head;
    l->head = tmp->next;
    l->size--;

    void* p = tmp->data;
    SAFE_RELEASE(tmp);

    return p;
}

ret_t fifo_release(fifo_t* l, bool data_release) {

    void* p = NULL;
    while ((p = fifo_pop(l))) {
        if (data_release)
            l->rel_cb(p);
    }

    SAFE_RELEASE(l);

    return ST_OK;

}

#ifndef NDEBUG
void test_fifo() {
    u64 ii[] = {0, 1, 2, 3, 4, 5, 6, 7};

    fifo_t* f = NULL;
    fifo_init(&f, NULL);

    fifo_push(f, &ii[0]);
    fifo_push(f, &ii[1]);
    fifo_push(f, &ii[2]);
    fifo_push(f, &ii[3]);
    fifo_push(f, &ii[4]);
    fifo_push(f, &ii[5]);
    fifo_push(f, &ii[6]);
    fifo_push(f, &ii[7]);

    u64* p = NULL;
    u64 i = 0;
    while ((p = (u64*)fifo_pop(f))) {
        ASSERT(ii[i++] == *p);
        LOG_TRACE("fifo %lu", *p);
    }

    fifo_release(f, false);

}

#endif
