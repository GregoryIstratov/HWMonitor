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

#include <memory.h>
#include <stdio.h>
#include "string.h"
#include "log.h"
#include "allocators.h"

ret_t regex_compile(regex_t* r, const char* pattern) {
    int status = regcomp(r, pattern, REG_EXTENDED | REG_NEWLINE);
    if (status != 0) {
        char error_message[MAX_ERROR_MSG];
        regerror(status, r, error_message, MAX_ERROR_MSG);

        LOG_ERROR("Regex error compiling '%s': %s",
                  pattern, error_message);
        return (ST_ERR);
    }
    return ST_OK;
}

bool regex_match(regex_t* r, const char* text) {
    regmatch_t m[10];
    int nomatch = regexec(r, text, 10, m, 0);
    if (nomatch) {
        return false;
    }

    return true;
}

//============================================================================================================
// STRING
//============================================================================================================

ret_t string_init(string** sp) {
    return da_init(_dap(sp));
}

ret_t string_release(string* s) {
    return da_release(_da(s));
}

void string_release_cb(void* p) {
    string_release((string*)p);
}

const char* string_cdata(string* s) {
    return _da(s)->ptr;
}

u64 string_size(string* s) {
    return _da(s)->used;
}

char string_char(string* s, u64 idx) {
    if (idx > string_size(s)) {
        LOG_ERROR("Index is out of bound %lu > %lu", idx, string_size(s));
        return (char)-1;
    }

    return _da(s)->ptr[idx];
}

ret_t string_create_nt(string* s, char** buff) {
    u64 ssize = string_size(s);
    *buff = zalloc(ssize + 1);
    memcpy(*buff, string_cdata(s), ssize);

    return ST_OK;
}

///deep copy
ret_t string_dub(string* s, string** ns) {
    return da_dub(_da(s), _dap(ns));
}

ret_t string_append(string* s, const char* str) {
    u64 len = strlen(str);
    return da_append(_da(s), str, len);

}

ret_t string_appendf(string* s, const char* fmt, ...) {
#define FORMAT_BUFFER_SIZE 4096
    va_list args;
    va_start(args, fmt);

    char buf[FORMAT_BUFFER_SIZE];
    memset(buf, 0, FORMAT_BUFFER_SIZE);
    vsnprintf(buf, FORMAT_BUFFER_SIZE, fmt, args);

    va_end(args);

    return string_append(s, buf);
#undef FORMAT_BUFFER_SIZE
}

ret_t string_append_se(string* s, const char* start, const char* end) {
    u64 sz = (u64)(end - start);
    return da_append(_da(s), start, sz);
}

char* string_makez(string* s) {
    if (!s)
        return NULL;

    u64 sz = _da(s)->used;
    char* data = zalloc(sz + 1);
    memcpy(data, _da(s)->ptr, sz);
    return data;
}

ret_t string_appendn(string* s, const char* str, u64 len) {
    return da_append(_da(s), str, len);

}

ret_t string_create(string** s, const char* str) {
    string_init(s);

    return string_append(*s, str);
}

ret_t string_add(string* a, string* b) {
    return da_concat(_da(a), _da(b));
}

ret_t string_pop_head(string* s, u64 n) {
    return da_pop_head(_da(s), n);
}

ret_t string_crop_tail(string* s, u64 n) {
    return da_crop_tail(_da(s), n);
}

u64 string_find_last_char(string* s, char ch) {
    u64 ssize = string_size(s);
    for (u64 i = ssize; i != 0; --i) {
        char cur = string_char(s, i);
        if (cur == ch)
            return i;
    }

    return (u64)-1;

}

ret_t string_remove_seq(string* s, u64 begin, u64 end) {
    return da_remove(_da(s), begin, end);
}

ret_t string_remove_dubseq(string* s, char delm, uint8_t skip) {
    u64 j = 0;
    while (j < string_size(s)) {
        const char* cur = string_cdata(s);

        if (cur[j] == delm) {
            u64 begin = j;
            u64 end = begin;

            while (cur[(++end)] == delm);

            //--end;// last compared position
            u64 n = end - begin;

            if (n > 1) {

                u64 skip_ = 0;
                if (skip >= n) {
                    LOG_WARN("skip value is too high. skip=%d", skip);
                } else {
                    skip_ = (u64)skip;
                }

                CHECK_RETURN(string_remove_seq(s, begin, end - skip_));
                j = 0; // skip due to internal buffer changed
                continue;
            }
        }

        ++j;
    }

    return ST_OK;
}

static const char strip_dict[] = {'\0', '\n', '\r', '\t', ' ', '"', '\"', '\''};
static const char strip_dict_ws[] = {'\0', '\n', '\r', '\t', ' '};

static inline bool check_strip_dict(char ch) {
    for (u64 i = 0; i < sizeof(strip_dict) / sizeof(char); ++i)
        if (ch == strip_dict[i])
            return true;

    return false;
}

ret_t string_rstrip(string* s) {

    if (string_size(s) < 1) {
        LOG_WARN("String size(%lu) is too small", string_size(s));
        return (ST_OUT_OF_RANGE);
    }

    u64 n = 0;
    for (u64 j = string_size(s) - 1; j != 0; --j) {
        char sch = string_char(s, j);
        if (check_strip_dict(sch))
            ++n;
        else
            break;
    }

    return string_pop_head(s, n);
}

ret_t string_lstrip(string* s) {

    u64 n = 0;
    for (u64 j = 0; j < string_size(s); ++j) {
        char sch = string_char(s, j);
        if (check_strip_dict(sch))
            ++n;
        else
            break;
    }

    return string_crop_tail(s, n);
}

ret_t string_strip(string* s) {

    if (string_size(s) < 3)
        return ST_SIZE_EXCEED;

    string_rstrip(s);
    string_lstrip(s);

    return ST_OK;
}

static inline bool check_strip_dict_ws(char ch) {
    for (u64 i = 0; i < sizeof(strip_dict_ws) / sizeof(char); ++i)
        if (ch == strip_dict_ws[i])
            return true;

    return false;
}

ret_t string_rstrip_ws(string* s) {
    while (string_size(s) > 2) {

        u64 idx = string_size(s) - 1;
        char sch = string_char(s, idx);

        if (check_strip_dict_ws(sch)) {
            string_pop_head(s, 1);
        } else {
            return ST_OK;
        }

    }

    return ST_OK;
}

ret_t string_starts_with(string* s, const char* str) {
    u64 str_len = strlen(str);
    if (str_len > string_size(s))
        return (ST_SIZE_EXCEED);

    if (memcmp(string_cdata(s), str, str_len) == 0)
        return (ST_OK);

    return (ST_NOT_FOUND);
}

bool string_re_match(string* s, const char* pattern) {
    regex_t re;
    regex_compile(&re, pattern);

    char* text = string_makez(s);
    bool m = regex_match(&re, text);
    zfree(text);
    regfree(&re);

    return m;
}

ret_t string_compare(string* a, string* b) {
    return da_compare(_da(a), _da(b));
}

ret_t string_comparez(string* a, const char* str) {
    u64 ssize = strlen(str);
    if (_da(a)->used > ssize)
        return ST_ERR;
    else if (_da(a)->used < ssize)
        return ST_ERR;
    else {
        if (memcmp(_da(a)->ptr, str, ssize) == 0)
            return ST_OK;
        else
            return ST_ERR;
    }
}

ret_t string_map_region(string* s, u64 beg, u64 end, char** sb, char** se) {
    u64 ssize = string_size(s);
    if ((beg > 0 && beg <= ssize) && (end > 0 && end <= ssize)) {
        LOG_ERROR("Indexes are out of bound");
        return ST_OUT_OF_RANGE;
    }

    *sb = _da(s)->ptr + beg;
    *se = _da(s)->ptr + end;

    return ST_OK;
}

void string_map_string(string* s, char** sb, char** se) {
    string_map_region(s, 0, string_size(s), sb, se);
}

ret_t string_to_u64(string* s, u64* ul) {

    u64 res = 0;
    u64 ssize = string_size(s);
    for (u64 i = 0; i < ssize; ++i)
        res = res * 10UL + (u64)(string_char(s, i) - '0');

    *ul = res;

    return ST_OK;

}

void string_pair_release_cb(void* p) {
    if (!p)
        return;

    string_pair_t* pair = (string_pair_t*)p;

    if (pair->first)
        string_release(pair->first);
    if (pair->second)
        string_release(pair->second);

    zfree(pair);
}

ret_t string_re_search(string* s, const char* pattern, list_t** pairs) {
    regex_t re;
    ret_t ret;
    if ((ret = regex_compile(&re, pattern)) != ST_OK) {
        return ret;
    }

    list_init(pairs, &string_release_cb);

    const char* tkp = _da(s)->ptr;
    const char* p = _da(s)->ptr;
    /* "N_matches" is the maximum number of matches allowed. */
#define n_matches 10
    /* "M" contains the matches found. */
    regmatch_t m[n_matches];

    while (1) {
        m[0].rm_so = 0;
        m[0].rm_eo = (int)(string_size(s) - (u64)(p - tkp));
        int nomatch = regexec(&re, p, n_matches, m, REG_STARTEND);
        if (nomatch) {
            LOG_INFO("No more matches.");
            break;
        } else {

            for (int i = 0; i < n_matches; i++) {
                int start;
                int finish;
                if (m[i].rm_so == -1) {
                    break;
                }

                start = m[i].rm_so + (int)(p - tkp);
                finish = m[i].rm_eo + (int)(p - tkp);
                if (i == 0) {
                    continue;
                } else {
                    string* val;
                    string_init(&val);
                    string_appendn(val, tkp + start, (u64)(finish - start));

                    list_push(*pairs, val);
                }
            }

            p += m[0].rm_eo;

        }
    }

    regfree(&re);

    return ST_OK;
#undef n_matches
}

void test_string() {
    const char* str1 = "Hello, World!";
    const char* str2 = "What's up, Dude?";
    ASSERT(sizeof(dynamic_allocator_t) == sizeof(string));

    string* a = NULL;
    string* b = NULL;
    string* c = NULL;
    string* d = NULL;

    CHECK_RETURN(string_init(&a));
    CHECK_RETURN(string_release(a));
    a = NULL;

    CHECK_RETURN(string_create(&a, str1));
    CHECK_RETURN(string_comparez(a, str1));

    CHECK_RETURN(string_dub(a, &b));
    ASSERT(string_compare(a, b) == 0);

    CHECK_RETURN(string_append(b, str2));

    string_print(b);

    CHECK_RETURN(string_add(b, a));

    string_printt(a);
    string_print(b);

    CHECK_RETURN(string_create(&c, "      abc\n\n\n\n\n\n\n       "));

    CHECK_RETURN(string_strip(c));

    string_print(c);

    CHECK_RETURN(string_comparez(c, "abc"));

    CHECK_RETURN(string_create(&d, "aabccc____3_2_1   :::"));
    CHECK_RETURN(string_remove_dubseq(d, 'a', 0));
    CHECK_RETURN(string_remove_dubseq(d, 'b', 0));
    CHECK_RETURN(string_remove_dubseq(d, 'c', 0));
    CHECK_RETURN(string_remove_dubseq(d, '_', 0));
    CHECK_RETURN(string_remove_dubseq(d, '3', 0));
    CHECK_RETURN(string_remove_dubseq(d, '2', 0));
    CHECK_RETURN(string_remove_dubseq(d, '1', 0));
    CHECK_RETURN(string_remove_dubseq(d, ' ', 0));
    CHECK_RETURN(string_remove_dubseq(d, ':', 0));

    ASSERT(string_comparez(d, "b3_2_1") == ST_OK);
    string_print(d);

    string_release(a);
    string_release(b);
    string_release(c);
    string_release(d);

}

//============================================================================================================
// SLIST
//============================================================================================================

void slist_fprintd(list_t* sl) {

    if (!sl) {
        LOG_WARN("sl == NULL");
        return;
    }

    list_node_t* head = sl->head;
    while (head) {
        string* s = (string*)head->data;
#ifdef NDEBUG
        string_print(s);
#else
        string_printd(s);
#endif

        head = head->next;
    }
}

void slist_rfprintd(list_t* sl) {
    if (!sl) {
        LOG_WARN("sl == NULL");
        return;
    }

    list_node_t* tail = sl->tail;

    while (tail) {
        string* s = (string*)tail->data;
#ifdef NDEBUG
        string_print(s);
#else
        string_printd(s);
#endif
        tail = tail->prev;
    }
}

string* slist_next(list_iter_t* it) {
    return (string*)list_iter_next(it);
}

ret_t string_split(string* s, char delm, list_t** l) {
    if (string_size(s) == 0)
        return ST_EMPTY;

    string_rstrip_ws(s);
    string_remove_dubseq(s, delm, 1);

    list_init(l, &string_release_cb);
    char* cb = NULL;
    char* end = NULL;

    string_map_string(s, &cb, &end);
    char* ccur = cb;

    while (ccur <= end) {
        if (*ccur == delm || ccur == end) {
            //while(*(++ccur) == delm && ccur == end);


            string* ss = NULL;
            string_init(&ss);
            string_appendn(ss, cb, (u64)(ccur - cb));

            list_push(*l, ss);

            cb = ++ccur;
        }

        ++ccur;
    }

    return ST_OK;
}

