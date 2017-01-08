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
#include "double_linked_list.h"
#include "log.h"
#include "allocators.h"

void list_iter_init(list_t* l, list_iter_t** it) {
    *it = zalloc(sizeof(list_iter_t));
    (*it)->node = l->head;
}

void list_iter_release(list_iter_t* it) {
    SAFE_RELEASE(it);
}

void* list_iter_next(list_iter_t* it) {
    if (it->node == NULL)
        return NULL;

    void* p = it->node->data;
    it->node = it->node->next;

    return p;
}

void list_init(list_t** l, data_release_cb cb) {
    *l = zalloc(sizeof(list_t));
    (*l)->rel_cb = cb;
}

void list_node_init(list_node_t** node) {
    *node = zalloc(sizeof(list_node_t));
}

void list_push(list_t* l, void* s) {

    if (!l->head) {
        list_node_init(&l->head);

        list_node_t* node = l->head;

        node->data = s;

        l->tail = l->head;
    } else {
        list_node_t* node = NULL;
        list_node_init(&node);
        node->data = s;

        list_node_t* tail = l->head;
        while (tail->next) {
            tail = tail->next;
        }

        tail->next = node;

        node->prev = tail;

        l->tail = node;

    }

    ++l->size;
}

void* list_pop_head(list_t* l) {
    if (l->size == 0)
        return NULL;

    list_node_t* tmp = l->head;
    l->head = tmp->next;
    l->size--;

    void* data = tmp->data;
    zfree(tmp);

    return data;
}

void* list_crop_tail(list_t* l) {
    if (l->size == 0)
        return NULL;

    list_node_t* tmp = l->tail;
    l->tail = tmp->prev;
    l->size--;

    void* data = tmp->data;
    zfree(tmp);

    return data;
}

ret_t list_release(list_t* l, bool release_data) {
    if (!l)
        return ST_EMPTY;

    list_node_t* head = l->head;
    while (head) {

        list_node_t* tmp = head;

        if (release_data)
            l->rel_cb(tmp->data);

        head = head->next;

        zfree(tmp);
    }

    zfree(l);

    return ST_OK;

}

ret_t list_merge(list_t* __restrict a, list_t* __restrict b) {
    if (a->size == 0 && b->size == 0)
        return ST_ERR;

    a->tail->next = b->head;
    b->head->prev = a->tail;
    a->size += b->size;
    a->tail = b->tail;

    return ST_OK;
}

//TODO segfault
ret_t list_remove(list_t* l, const void* data) {
    list_node_t* head = l->head;

    while (head) {
        if (head->data == data) {
            list_node_t* hn = head->next;
            list_node_t* hp = head->prev;

            hn->prev->next = hp;
            hp->next->prev = hn;

            zfree(head);
            --l->size;

            return ST_OK;

        }

        head = head->next;
    }

    return (ST_NOT_FOUND);
}

typedef void(* list_traverse_cb)(list_node_t*);

ret_t list_traverse(list_t* l, bool forward, list_traverse_cb cb) {
    if (!l)
        return ST_EMPTY;

    list_node_t* cur = forward ? l->tail : l->head;

    while (cur) {
        cb(cur);
        cur = forward ? cur->prev : cur->next;
    }

    return ST_OK;
}

#ifndef NDEBUG
static inline void test_list_traverse(list_node_t* node) {
    u64 i = *((u64*)node->data);
    LOG_TRACE("list elem %lu", i);
}

void test_list() {
    u64 ii[] = {0, 1, 2, 3, 4, 5, 6, 7};
    list_t* l = NULL;
    list_init(&l, NULL);

    ASSERT(l != NULL);

    list_push(l, &ii[0]);
    list_push(l, &ii[1]);
    list_push(l, &ii[2]);
    list_push(l, &ii[3]);
    list_push(l, &ii[4]);
    list_push(l, &ii[5]);
    list_push(l, &ii[6]);
    list_push(l, &ii[7]);

    ASSERT(l->size == 8);

    list_traverse(l, false, &test_list_traverse);
    list_traverse(l, true, &test_list_traverse);

    list_node_t* head = l->tail;
    u64 i = 7;
    while (head) {
        u64* pi = (u64*)head->data;

        LOG_TRACE("list elem i[%lu] = %lu", i, *pi);
        ASSERT(ii[i] == *pi);

        --i;
        head = head->prev;
    }

    i = 0;
    u64* pdata = NULL;
    while ((pdata = list_pop_head(l))) {
        LOG_TRACE("list elem i[%lu] = %lu", i, *pdata);
        ASSERT(ii[i] == *pdata);

        ++i;
    }

    list_release(l, false);
    l = NULL;
    ASSERT(l == NULL);

    list_init(&l, NULL);

    ASSERT(l != NULL);

    list_push(l, &ii[0]);
    list_push(l, &ii[1]);
    list_push(l, &ii[2]);
    list_push(l, &ii[3]);
    list_push(l, &ii[4]);
    list_push(l, &ii[5]);
    list_push(l, &ii[6]);
    list_push(l, &ii[7]);

    ASSERT(l->size == 8);

    head = l->head;
    i = 0;
    while (head) {
        u64* pi = (u64*)head->data;

        LOG_TRACE("list elem i[%lu] = %lu", i, *pi);
        ASSERT(ii[i] == *pi);

        ++i;
        head = head->next;
    }

    i = 0;
    while ((pdata = list_pop_head(l))) {
        LOG_TRACE("list elem i[%lu] = %lu", i, *pdata);
        ASSERT(ii[i] == *pdata);

        ++i;
    }

    list_release(l, false);
    l = NULL;
    ASSERT(l == NULL);

//// remove test failed
//    list_init(&l, sizeof(u64));
//
//    ASSERT(l != NULL);
//
//    list_push(l, &ii[0]);
//    list_push(l, &ii[1]);
//    list_push(l, &ii[2]);
//    list_push(l, &ii[3]);
//    list_push(l, &ii[4]);
//    list_push(l, &ii[5]);
//    list_push(l, &ii[6]);
//    list_push(l, &ii[7]);
//
//
//    list_remove(l, &ii[1]);
//    list_remove(l, &ii[3]);
//    list_remove(l, &ii[5]);
//    list_remove(l, &ii[7]);
//
//    ASSERT(l->size == 4);
//
//    head = l->head;
//    while(head)
//    {
//        u64* pi = (u64*)head->data;
//
//        LOG_TRACE("list elem i[%lu] = %lu", i, *pi);
//        ASSERT((*pi % 2) == 0);
//
//        head = head->next;
//    }


    list_t* l2 = NULL;
    list_init(&l, NULL);
    list_init(&l2, NULL);

    ASSERT(l != NULL);
    ASSERT(l2 != NULL);

    list_push(l, &ii[0]);
    list_push(l, &ii[1]);
    list_push(l, &ii[2]);
    list_push(l, &ii[3]);
    list_push(l2, &ii[4]);
    list_push(l2, &ii[5]);
    list_push(l2, &ii[6]);
    list_push(l2, &ii[7]);

    list_merge(l, l2);

    head = l->head;
    i = 0;
    while (head) {
        u64* pi = (u64*)head->data;

        LOG_TRACE("list elem i[%lu] = %lu", i, *pi);
        ASSERT(ii[i] == *pi);

        ++i;
        head = head->next;
    }

    head = l->tail;
    i = 7;
    while (head) {
        u64* pi = (u64*)head->data;

        LOG_TRACE("list elem i[%lu] = %lu", i, *pi);
        ASSERT(ii[i] == *pi);

        --i;
        head = head->prev;
    }

    list_release(l, false);
    l = NULL;
    ASSERT(l == NULL);
    list_release(l2, false);
    l2 = NULL;
    ASSERT(l2 == NULL);
}

#endif
