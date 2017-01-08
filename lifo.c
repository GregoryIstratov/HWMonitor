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
#include "lifo.h"
#include "log.h"
#include "allocators.h"

void lifo_init(lifo_t** l, data_release_cb cb) {
    *l = zalloc(sizeof(lifo_t));
    (*l)->rel_cb = cb;
}

void lifo_node_init(lifo_node_t** node) {
    *node = zalloc(sizeof(lifo_node_t));
}

void lifo_push(lifo_t* l, void* s) {

    if (!l->head) {
        lifo_node_init(&l->head);

        lifo_node_t* node = l->head;

        node->data = s;
    } else {
        lifo_node_t* node = NULL;
        lifo_node_init(&node);
        node->data = s;
        node->prev = l->head;

        l->head = node;
    }

    ++l->size;
}

void* lifo_pop(lifo_t* l) {
    if (!l->head)
        return NULL;

    lifo_node_t* tmp = l->head;
    l->head = tmp->prev;
    l->size--;

    void* p = tmp->data;
    SAFE_RELEASE(tmp);

    return p;
}

ret_t lifo_release(lifo_t* l, bool data_release) {

    void* p = NULL;
    while ((p = lifo_pop(l))) {
        if (data_release)
            l->rel_cb(p);
    }

    SAFE_RELEASE(l);

    return ST_OK;

}

#ifndef NDEBUG

void test_lifo() {
    static u64 ii[] = {0, 1, 2, 3, 4, 5, 6, 7};

    lifo_t* f = NULL;
    lifo_init(&f, NULL);

    lifo_push(f, &ii[0]);
    lifo_push(f, &ii[1]);
    lifo_push(f, &ii[2]);
    lifo_push(f, &ii[3]);
    lifo_push(f, &ii[4]);
    lifo_push(f, &ii[5]);
    lifo_push(f, &ii[6]);
    lifo_push(f, &ii[7]);

    u64* p = NULL;
    u64 i = 7;
    while ((p = (u64*)lifo_pop(f))) {
        ASSERT(ii[i--] == *p);
        LOG_TRACE("lifo %lu", *p);
    }

    lifo_release(f, false);

}

#endif
