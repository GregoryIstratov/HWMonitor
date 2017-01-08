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

#include "mem_dev.h"
#include "double_linked_list.h"
#include "string.h"
#include "utils.h"
#include "allocators.h"

void mem_info_release_cb(void* p) {
    if (!p)
        return;

    mem_info_t* mem = (mem_info_t*)p;

    zfree(mem);
}

void mem_info_get(mem_info_t** mem_info) {
    *mem_info = zalloc(sizeof(mem_info_t));
    mem_info_t* m = *mem_info;

    string* mem_info_s = NULL;
    string_init(&mem_info_s);
    file_read_all_buffered_s("/proc/meminfo", mem_info_s);

    list_t* pairs = NULL;

    string_re_search(mem_info_s, "([a-zA-Z]+):\\s+([0-9]+)", &pairs);

    string_release(mem_info_s);

    u64 i = 0;

    while (i < pairs->size) {
        ++i;
        string* key = (string*)list_pop_head(pairs);
        string* val = (string*)list_pop_head(pairs);

        if (string_comparez(key, "MemTotal") == ST_OK) {
            string_to_u64(val, &m->mem_total);
            m->mem_total *= 1024;
        } else if (string_comparez(key, "MemFree") == ST_OK) {
            string_to_u64(val, &m->mem_free);
            m->mem_free *= 1024;
        } else if (string_comparez(key, "MemAvailable") == ST_OK) {
            string_to_u64(val, &m->mem_avail);
            m->mem_avail *= 1024;
        } else if (string_comparez(key, "Cached") == ST_OK) {
            string_to_u64(val, &m->cached);
            m->cached *= 1024;
        } else if (string_comparez(key, "SwapCached") == ST_OK) {
            string_to_u64(val, &m->swap_cached);
            m->swap_cached *= 1024;
        } else if (string_comparez(key, "SwapTotal") == ST_OK) {
            string_to_u64(val, &m->swap_total);
            m->swap_total *= 1024;
        } else if (string_comparez(key, "SwapFree") == ST_OK) {
            string_to_u64(val, &m->swap_free);
            m->swap_free *= 1024;

            string_release(key);
            string_release(val);
            break;
        }

        string_release(key);
        string_release(val);
    }

    list_release(pairs, true);
}
