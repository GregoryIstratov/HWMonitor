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

#include <regex.h>

#include "globals.h"
#include "double_linked_list.h"
#include "dynamic_allocator.h"

//============================================================================================================
// REGEX
//============================================================================================================

#define MAX_ERROR_MSG 0x1000

ret_t regex_compile(regex_t* r, const char* pattern);

bool regex_match(regex_t* r, const char* text);

//============================================================================================================
// STRING
//============================================================================================================

#define string_print(a) LOG_INFO("%.*s", string_size(a), string_cdata(a));
#define string_printd(a) LOG_DEBUG("%.*s", string_size(a), string_cdata(a));
#define string_printt(a) LOG_TRACE("%.*s", string_size(a), string_cdata(a));

typedef struct {
    union {
        uint8_t _[sizeof(dynamic_allocator_t)];
        dynamic_allocator_t da;
    };

} string;

#define _da(x) ((dynamic_allocator_t*)(x))
#define _dap(x) ((dynamic_allocator_t**)(x))

typedef struct skey_value {
    string* key;
    string* value;
} skey_value_t;

ret_t string_init(string** sp);

ret_t string_release(string* s);

void string_release_cb(void* p);

const char* string_cdata(string* s);

u64 string_size(string* s);

char string_char(string* s, u64 idx);

ret_t string_create_nt(string* s, char** buff);

/// deep copy
/// \param s input string
/// \param ns output string
/// \return return code
ret_t string_dub(string* s, string** ns);

ret_t string_append(string* s, const char* str);

ret_t string_appendf(string* s, const char* fmt, ...);

ret_t string_append_se(string* s, const char* start, const char* end);

char* string_makez(string* s);

ret_t string_appendn(string* s, const char* str, u64 len);

ret_t string_create(string** s, const char* str);

ret_t string_add(string* a, string* b);

ret_t string_pop_head(string* s, u64 n);

ret_t string_crop_tail(string* s, u64 n);

u64 string_find_last_char(string* s, char ch);

ret_t string_remove_seq(string* s, u64 begin, u64 end);

ret_t string_remove_dubseq(string* s, char delm, uint8_t skip);

ret_t string_rstrip(string* s);

ret_t string_lstrip(string* s);

ret_t string_strip(string* s);

ret_t string_rstrip_ws(string* s);

ret_t string_starts_with(string* s, const char* str);

bool string_re_match(string* s, const char* pattern);

ret_t string_compare(string* a, string* b);

ret_t string_comparez(string* a, const char* str);

ret_t string_map_region(string* s, u64 beg, u64 end, char** sb, char** se);

void string_map_string(string* s, char** sb, char** se);

ret_t string_to_u64(string* s, u64* ul);

typedef struct string_pair {
    string* first;
    string* second;
} string_pair_t;

void string_pair_release_cb(void* p);

ret_t string_re_search(string* s, const char* pattern, list_t** pairs);

void test_string(void);

//============================================================================================================
// SLIST
//============================================================================================================

void slist_fprintd(list_t* sl);

void slist_rfprintd(list_t* sl);

string* slist_next(list_iter_t* it);

ret_t string_split(string* s, char delm, list_t** l);
