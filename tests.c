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

#include "tests.h"
#include "globals.h"
#include "string.h"
#include "log.h"
//============================================================================================================
// TESTS
//============================================================================================================

#ifndef NDEBUG

static ret_t test_split(void);

extern void test_hash_bt(void);
extern void test_fifo(void);
extern void test_lifo(void);

void tests_run() {
    test_da();
    test_string();
    test_hash_bt();
    test_fifo();
    test_lifo();

    //TODO test_list breaks the memory
    //test_list();

    test_split();
}

static ret_t test_split() {

    string* text = NULL;
    string_init(&text);

    string_append(text, "SIZE=\"240057409536\" MODEL=\"TOSHIBA-TR150   \" LABEL=\"\" UUID=\"\" MOUNTPOINT=\"\"\n");
    string_append(text,
                  "NAME=\"sda1\" FSTYPE=\"vfat\" SCHED=\"cfq\" SIZE=\"536870912\" MODEL=\"\" LABEL=\"\" UUID=\"B58E-8A00\" MOUNTPOINT=\"/boot\"\n");
    string_append(text,
                  "NAME=\"sda2\" FSTYPE=\"swap\" SCHED=\"cfq\" SIZE=\"17179869184\" MODEL=\"\" LABEL=\"\" UUID=\"c8ae3239-f359-4bff-8994-c78d20efd308\" MOUNTPOINT=\"[SWAP]\"\n");
    string_append(text,
                  "NAME=\"sda3\" FSTYPE=\"ext4\" SCHED=\"cfq\" SIZE=\"42949672960\" MODEL=\"\" LABEL=\"\" UUID=\"ecff6ff7-1380-44df-a1a5-e2a4e10eba4e\" MOUNTPOINT=\"/\"\n");
    string_append(text,
                  "NAME=\"sda4\" FSTYPE=\"ext4\" SCHED=\"cfq\" SIZE=\"179389931008\" MODEL=\"\" LABEL=\"\" UUID=\"e77e913c-9829-4750-b3ee-ccf4e641d67a\" MOUNTPOINT=\"/home\"\n");
    string_append(text,
                  "NAME=\"sdb\" FSTYPE=\"\" SCHED=\"cfq\" SIZE=\"2000398934016\" MODEL=\"ST2000DM001-1CH1\" LABEL=\"\" UUID=\"\" MOUNTPOINT=\"\"\n");
    string_append(text,
                  "NAME=\"sdb1\" FSTYPE=\"ext4\" SCHED=\"cfq\" SIZE=\"2000397868544\" MODEL=\"\" LABEL=\"\" UUID=\"cdc9e724-a78b-4a25-9647-ad6390e235c3\" MOUNTPOINT=\"\"\n");

    list_t* blklist = NULL;
    string_split(text, '\n', &blklist);

    LOG_TRACE("------- LINES OF TOKENS -----------");
    slist_fprintd(blklist);

    string* tk = NULL;
    list_iter_t* blkit = NULL;
    list_iter_init(blklist, &blkit);

    while ((tk = slist_next(blkit)) != NULL) {

        LOG_TRACE("------- TOKEN LINE-----------");
        string_printt(tk);

        LOG_TRACE("------- TOKEN SPLIT-----------");

        list_t* tokens = NULL;
        string_split(tk, ' ', &tokens);

        slist_fprintd(tokens);

        string* s = NULL;
        list_iter_t* token_it = NULL;
        list_iter_init(blklist, &token_it);

        while ((s = slist_next(token_it)) != NULL) {
            list_t* kv = NULL;
            string_split(s, '=', &kv);

            LOG_TRACE("------- TOKEN KEY-VALUE -----------");
            slist_fprintd(kv);

            string* key = (string*)kv->head->data;
            string* val = (string*)kv->head->next->data;

            string_strip(val);

            LOG_TRACE("------- STRIPED TOKEN KEY-VALUE -----------");
            string_printt(key);
            LOG_TRACE("(null)");

            list_release(kv, true);
            kv = NULL;
        }

        list_iter_release(token_it);
        list_release(tokens, true);
        tokens = NULL;

    }

    list_iter_release(blkit);
    list_release(blklist, true);
    blklist = NULL;
    string_release(text);
    text = NULL;

    return ST_OK;

}

#endif
