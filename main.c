/**************************************************************************************
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
                        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************************/


#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <bits/mman.h>
#include <ctype.h>
#include <string.h>
#include <regex.h>
#include <pthread.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdatomic.h>

//external
#include <blkid/blkid.h> // apt install libblkid-dev
#include <ncurses.h>     // apt install libncurses5-dev

//=======================================================================
// SETTINGS
//=======================================================================

#define PTR_TO_U64(ptr) (uint64_t)(uint64_t*)ptr
#define SAFE_RELEASE(x) { if(x){ free(x); x = NULL; } }

#define HW_VERSION_MAJOR 0
#define HW_VERSION_MINOR 1
#define STRING_INIT_BUFFER 4
#define ALLOC_ALIGN 16

#define HASHTABLE_SIZE 16

#define DA_MAX_MULTIPLICATOR 4

#define KiB 1024UL
#define MiB 1048576UL
#define GiB 1073741824UL

#define DEVICE_BASE_SAMPLE_RATE 0.1

//=======================================================================
// LOG
//=======================================================================

#define ENABLE_LOGGING

#ifndef ENABLE_LOGGING
#define LOG_ERROR(...) (_log(stderr, __VA_ARGS__))
#define LOG_WARN(...) (_log(stdout, __VA_ARGS__))
#define LOG_DEBUG(...) (_log(stdout, __VA_ARGS__))
#define LOG_INFO(...) (_log(stdout, __VA_ARGS__))
#define LOG_TRACE(...) (_log(stdout, __VA_ARGS__))
#define LOG_ASSERT(...) (_log(stdout, __VA_ARGS__))
#define ASSERT(exp) (assert(exp))


static void _log(FILE* out, ...)
{
    va_list args;
    va_start(args, out);
    const char *fmt = va_arg(args, const char*);

    vfprintf(out, fmt, args);
    fputc('\n', out);

    va_end(args);
}


#else

#ifndef NDEBUG
#define LOG_ERROR(...) (_log(__FILE__, __LINE__, __func__, LOG_ERROR, __VA_ARGS__))
#define LOG_WARN(...) (_log(__FILE__, __LINE__, __func__, LOG_WARN, __VA_ARGS__))
#define LOG_DEBUG(...) (_log(__FILE__, __LINE__, __func__, LOG_DEBUG, __VA_ARGS__))
#define LOG_INFO(...) (_log(__FILE__, __LINE__, __func__, LOG_INFO, __VA_ARGS__))
#define LOG_TRACE(...) (_log(__FILE__, __LINE__, __func__, LOG_TRACE, __VA_ARGS__))
#define LOG_ASSERT(...) (_log(__FILE__, __LINE__, __func__, LOG_ASSERT, __VA_ARGS__))
#define ASSERT(exp) ((exp)?__ASSERT_VOID_CAST (0): _log(__FILE__, __LINE__, __func__, LOG_ASSERT, #exp))
#define ASSERT_EQ(a, b) ((a == b)?__ASSERT_VOID_CAST (0): LOG_ASSERT("%s != %s [%lu] != [%lu]", #a, #b, a, b))
#else
#define LOG_ERROR(...) (_log(__FILE__, __LINE__, __func__, LOG_ERROR, __VA_ARGS__))
#define LOG_WARN(...) (_log(__FILE__, __LINE__, __func__, LOG_WARN, __VA_ARGS__))
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_TRACE(...) ((void)0)
#define LOG_ASSERT(...) ((void)0)
#define ASSERT(exp) ((void)0)
#define ASSERT_EQ(a, b) ((void)0)
#endif

#define LOG_SHOW_TIME
#define LOG_SHOW_DATE
#define LOG_SHOW_THREAD
#define LOG_SHOW_PATH
#define LOG_ENABLE_MULTITHREADING

#define LOG_FORMAT_BUFFER_MAX_SIZE 12400


#define LOG_RED   "\x1B[31m"
#define LOG_GRN   "\x1B[32m"
#define LOG_YEL   "\x1B[33m"
#define LOG_BLU   "\x1B[34m"
#define LOG MAG   "\x1B[35m"
#define LOG_CYN   "\x1B[36m"
#define LOG_WHT   "\x1B[37m"
#define LOG_RESET "\x1B[0m"

enum {
    LOGLEVEL_NONE = 0,
    LOGLEVEL_WARN,
    LOGLEVEL_INFO,
    LOGLEVEL_DEBUG,
    LOGLEVEL_TRACE,
    LOGLEVEL_ALL = 0xFFFFFF
};

enum {
    LOG_ERROR = 0,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_ASSERT

};

#ifdef LOG_ENABLE_MULTITHREADING
static pthread_spinlock_t stderr_spinlock;
static pthread_spinlock_t stdout_spinlock;
#endif

static int loglevel = LOGLEVEL_DEBUG;
static FILE *stdlog = NULL;

static void log_init(int loglvl, const char *filename) {
    loglevel = loglvl;
#ifdef LOG_ENABLE_MULTITHREADING
    pthread_spin_init(&stderr_spinlock, 0);
    pthread_spin_init(&stdout_spinlock, 0);
#endif
    stdlog = fopen(filename, "a");
}

static void log_shitdown() {
    if (stdlog) fclose(stdlog);
}

static const char *loglevel_s(int lvl) {
    switch (lvl) {
        case LOG_ERROR:
            return "ERR";
        case LOG_WARN:
            return "WRN";
        case LOG_DEBUG:
            return "DBG";
        case LOG_INFO:
            return "INF";
        case LOG_TRACE:
            return "TRC";
        case LOG_ASSERT:
            return "ASSERTION FAILED";
        default:
            return "UNKNOWN";
    }
}

static const char *log_color(int lvl) {
    switch (lvl) {
        case LOG_ERROR:
            return LOG_RED;
        case LOG_DEBUG:
            return LOG_CYN;
        case LOG_INFO:
            return LOG_GRN;
        case LOG_TRACE:
            return LOG_WHT;
        case LOG_WARN:
            return LOG_YEL;
        case LOG_ASSERT:
            return LOG_YEL;
        default:
            return LOG_RESET;
    }
}

static void _log(const char *file, int line, const char *fun, int lvl, ...) {
    if (lvl <= loglevel) {

#ifdef LOG_SHOW_THREAD
        pid_t tid = syscall(__NR_gettid);
#endif

        va_list args;
        va_start(args, lvl);
        const char *fmt = va_arg(args, const char*);

        char buf[LOG_FORMAT_BUFFER_MAX_SIZE];
        memset(buf, 0, LOG_FORMAT_BUFFER_MAX_SIZE);
        vsnprintf(buf, LOG_FORMAT_BUFFER_MAX_SIZE, fmt, args);

        va_end(args);


#ifdef LOG_ENABLE_MULTITHREADING
        pthread_spin_lock(&stdout_spinlock);
#endif

#if defined LOG_SHOW_TIME || defined LOG_SHOW_DATE
        time_t t;
        struct tm _tml;
        struct tm *tml;
        if (time(&t) == (time_t) -1) {
            LOG_ERROR("time return failed");
            return;
        }

        localtime_r(&t, &_tml);
        tml = &_tml;

#endif

        fprintf(stdlog, "%s", log_color(lvl));
#ifdef LOG_SHOW_TIME
        fprintf(stdlog, "[%02d:%02d:%02d]", tml->tm_hour, tml->tm_min, tml->tm_sec);
#endif
#ifdef LOG_SHOW_DATE
        fprintf(stdlog, "[%02d/%02d/%d]", tml->tm_mday, tml->tm_mon + 1, tml->tm_year - 100);
#endif
#ifdef LOG_SHOW_THREAD
        fprintf(stdlog, "[0x%08x]", tid);
#endif

        fprintf(stdlog, "[%s][%s]: %s%s", loglevel_s(lvl), fun, buf, LOG_RESET);

#ifdef LOG_SHOW_PATH
        fprintf(stdlog, " - %s:%d", file, line);
#endif
        fprintf(stdlog, "\n");
        fflush(stdlog);

#ifdef LOG_ENABLE_MULTITHREADING
        pthread_spin_unlock(&stdout_spinlock);
#endif

    }
}

#endif // ENABLE_LOGGING
//===============================================================
// GLOBAS
//===============================================================
enum {
    ST_OK = 0,
    ST_ERR,
    ST_NOT_FOUND,
    ST_EMPTY,
    ST_EXISTS,
    ST_OUT_OF_RANGE,
    ST_SIZE_EXCEED,
    ST_UNKNOWN
};


typedef void(*data_release_cb)(void *p);

//======================================================================================================================
// RETURN MSG
//======================================================================================================================
// 64 bit
// [16 bit reserved][16 bit value data][16 bit idx msg][8 bit depth][8 bit code]

#define RETURN_CHECK(x) { ret_f_t r = return_map(x); \
if(r.code!=ST_OK) LOG_WARN("Return result code(%d) is not ok. User_data=%d. Msg: (%s). Depth=%d", r.code, r.data, gmsg_tab[r.idx], r.depth); \
return return_forward(x); }

#define CHECK_RETURN(x) { ret_f_t r = return_map(x); \
if(r.code!=ST_OK) LOG_ASSERT("[%s] Return result code(%d) is not ok. User_data=%d. Msg: (%s). Depth=%d", #x, r.code, r.data, gmsg_tab[r.idx], r.depth);  }

#define RET_MULTITHREADED

typedef uint64_t ret_t;

typedef struct ret_field {
    union {
        struct {
            uint8_t code;
            uint8_t depth;
            uint16_t idx;
            uint16_t data;
            uint16_t reserved;
        };
        ret_t r;
    };

} ret_f_t;

static const ret_t RET_OK = 0;

pthread_spinlock_t gmsg_tab_lock;
static const char *gmsg_tab[UINT16_MAX]; // about 500 kb, //TODO make it dynamic
static uint16_t gmsg_idx = 1;

#define return_trace(ret) LOG_TRACE("[%016d][%016d][%08d][%016d][%08d]", ret.reserved, \
 ret.data, ret.depth, ret.idx, ret.code);


static ret_t return_create(uint8_t code) {

    ret_f_t ret = {.code = code};

    return ret.r;
}

static ret_t return_create2(uint8_t code, uint16_t user_data) {
    ret_f_t ret = {.code =  code};
    ret.depth = 1;
    ret.data = user_data;

    return_trace(ret);

    return ret.r;
}

static ret_f_t return_map(ret_t r) {
    ret_f_t ret;
    ret.r = r;
    return ret;
}

static uint8_t return_code(ret_t r) {
    ret_f_t ret = return_map(r);
    return ret.code;
}

static uint16_t return_data(ret_t r) {
    ret_f_t ret = return_map(r);
    return ret.data;
}

static ret_t return_create_mv(uint8_t code, const char *msg, uint16_t user_data) {

#ifdef RET_MULTITHREADED
    pthread_spin_lock(&gmsg_tab_lock);

    gmsg_tab[gmsg_idx++] = msg;

    pthread_spin_unlock(&gmsg_tab_lock);
#else
    gmsg_tab[gmsg_idx++] = msg;
#endif

    ret_f_t ret;
    ret.r = 0;
    ret.code = code;
    ret.idx = (uint16_t) (gmsg_idx - 1);
    ret.depth = 0;
    ret.data = user_data;

    return_trace(ret);

    return ret.r;
}

static ret_t return_forward(ret_t r) {
    ret_f_t ret;
    ret.r = r;

    return_trace(ret);

    ret.depth++;


    return_trace(ret);

    return ret.r;

}

typedef struct ret_info {
    uint8_t code;
    const char *msg;
    uint8_t depth;
    uint16_t user_data;
} ret_info_t;

static void return_unpack(ret_t r, ret_info_t *info) {
    ret_f_t ret;
    ret.r = r;
    info->code = ret.code;

    uint16_t idx = ret.idx;
#ifdef RET_MULTITHREADED
    pthread_spin_lock(&gmsg_tab_lock);

    info->msg = gmsg_tab[idx];

    pthread_spin_unlock(&gmsg_tab_lock);
#else
    info->msg = gmsg_tab[idx];
#endif

    info->depth = ret.depth;

    info->user_data = ret.data;
}

static ret_t _test_ret3() {
    return return_create_mv(ST_OUT_OF_RANGE, "Massive bounds", 463);
}

static ret_t _test_ret2() {
    return return_forward(_test_ret3());
}

static ret_t _test_ret1() {
    ret_t r = _test_ret2();

    return return_forward(r);
}

static void test_ret() {

    ASSERT_EQ(sizeof(ret_f_t), sizeof(ret_t));

    ret_t ret = _test_ret1();


    ret_info_t info;
    return_unpack(ret, &info);

    ASSERT_EQ(info.code, ST_OUT_OF_RANGE);
    ASSERT_EQ(info.depth, 2);
    ASSERT(strcmp(info.msg, "Massive bounds") == 0);
    ASSERT_EQ(info.user_data, 463);


    ret_f_t r2 = return_map(ST_OK);

    ASSERT_EQ(r2.code, ST_OK);
}

//======================================================================================================================
//
//======================================================================================================================

typedef struct ht_key {
    union {
        char s[8];
        uint64_t i;

    } u;
} ht_key_t;

typedef struct ht_value {
    void *ptr;
    size_t size;
} ht_value_t;

typedef struct _ht_item_t {
    ht_key_t *key;
    ht_value_t *value;
    uint64_t hash;
    struct _ht_item_t *next;
} ht_item_t;

typedef struct _hashtable_t {
    ht_item_t *table[HASHTABLE_SIZE];
} hashtable_t;

static ret_t ht_init(hashtable_t **ht);

static void ht_destroy(hashtable_t *ht);

static ret_t ht_set(hashtable_t *ht, ht_key_t *key, ht_value_t *value);

static ret_t ht_get(hashtable_t *ht, ht_key_t *key, ht_value_t **value);


typedef void(*ht_forach_cb)(uint64_t, ht_key_t *, ht_value_t *);

static ret_t ht_foreach(hashtable_t *ht, ht_forach_cb cb);

static ret_t ht_create_key_i(uint64_t keyval, ht_key_t **key) {
    *key = malloc(sizeof(ht_key_t));
    (*key)->u.i = keyval;

    return ST_OK;
}

static ret_t ht_create_value(void *p, size_t size, ht_value_t **value) {
    *value = malloc(sizeof(ht_value_t));
    (*value)->ptr = p;
    (*value)->size = size;

    return ST_OK;
}


//======================================================================================================================
//======================================================================================================================



static const size_t size_npos = (size_t) -1;
// init gloabls

static void string_init_globals();

static ret_t init_gloabls() {

#ifdef RET_MULTITHREADED
    pthread_spin_init(&gmsg_tab_lock, 0);
#endif
    //disable buffering for stdout
    //setvbuf(stdout, NULL, _IONBF, 0);

    string_init_globals();

    return 0;
}

static void string_shutdown_globals();

static ret_t globals_shutdown() {

#ifdef RET_MULTITHREADED
    pthread_spin_destroy(&gmsg_tab_lock);
#endif

    string_shutdown_globals();

    return ST_OK;
}


//======================================================================================================================
// blob operations
//======================================================================================================================
/**
typedef struct {
    uint64_t id;
    uint64_t ref;
    uint64_t size;
} object_t;

static void *object_create(size_t size) {
    size_t csize = size + sizeof(object_t);
    char *ptr = malloc(csize);
    object_t b;
    b.id = (uint64_t) ptr;
    b.ref = 1;
    b.size = size;

    memcpy(ptr, &b, sizeof(object_t));


    // registration
    ht_key_t *key = NULL;
    ht_create_key_i((uint64_t) ptr, &key);

    ht_value_t *val = NULL;
    ht_create_value(ptr, csize, &val);

    ht_set(alloc_table, key, val);
    //-------------

    return ptr + sizeof(object_t);
}

static void *object_share(void *a, size_t obj_size) {
    object_t *b = (object_t *) ((char *) a - obj_size);
    //atomic_fetch_add(&b->ref, 1);
    b->ref++;
    return a;
}

static void *object_copy(void *a, size_t obj_size) {
    object_t *c = (object_t *) ((char *) a - obj_size);

    size_t csize = c->size + sizeof(object_t);
    uint64_t *ptr = malloc(csize);
    object_t b;
    b.id = (uint64_t) ptr;
    b.ref = 1;
    b.size = c->size;

    memcpy(ptr, &b, sizeof(object_t));

    // registration
    ht_key_t *key = NULL;
    ht_create_key_i((uint64_t) ptr, &key);

    ht_value_t *val = NULL;
    ht_create_value(ptr, csize, &val);

    ht_set(alloc_table, key, val);
    //-------------

    return ptr + sizeof(object_t);
}

static void object_release(void *a, size_t obj_size) {
    object_t *c = (object_t *) ((char *) a - obj_size);

    if (c->ref > 0) {
        //atomic_fetch_add(&c->ref, -1);
        c->ref--;
    }
}

#define OBJECT_DECLARE() uint64_t __alive;
#define OBJECT_INIT(x) x->__alive = object_alive_value;
#define OBJECT_CREATE(type) (type*)(object_create(sizeof(type)));
#define OBJECT_SHARE(x, type) (type*)object_share(x, sizeof(type));
#define OBJECT_COPY(x, type) (type*)object_copy(x)
#define OBJECT_RELEASE(x, type) object_release(x, sizeof(type))

**/
//===============================================================
// ALLOCATORS
//===============================================================

/**
typedef struct alloc_stat {
    uint64_t size;
} alloc_stat_t;

static void _record_alloc_set(void *ptr, size_t size) {
    alloc_stat_t *stat = malloc(sizeof(alloc_stat_t));
    stat->size = size;

    ht_key_t *key = NULL;
    ht_create_key_i((uint64_t) (uint64_t *) ptr, &key);

    ht_value_t *val = NULL;
    ht_create_value(stat, sizeof(struct alloc_stat), &val);


    ht_set(alloc_table, key, val);
}

static ret_t _record_alloc_get(void *ptr, alloc_stat_t **stat) {
    ht_value_t *val = NULL;
    ht_key_t *key = calloc(sizeof(ht_key_t), 1);
    key->u.i = PTR_TO_U64(ptr);
    int res = ht_get(alloc_table, key, &val);
    free(key);
    if (res != ST_OK)
        return res;


    *stat = malloc(val->size);
    memcpy(*stat, val->ptr, val->size);

    return ST_OK;
}

**/

#define safe_free(p) if(p) { free(p); p = NULL; }


static void *zalloc(size_t size) {
    void *v = malloc(size);
    if (v == NULL) {
        LOG_ERROR("malloc returns null pointer [size=%lu]. Trying again...", size);
        return zalloc(size);
    }

    memset(v, 0, size);


    return v;

}


#if 0

//TODO rework this shit
//// zeros allocated memory
static void *allocz(void *dst, size_t size) {
    size_t asize = size + (size % ALLOC_ALIGN);
    char *v = realloc(dst, asize);

    if (dst == NULL)
        memset(v, 0, asize);

//    if(dst) {
//        alloc_stat_t *stat = NULL;
//
//        if (_record_alloc_get(dst, &stat) == ST_OK) {
//            LOG_TRACE("found hash: 0x%08lx old_size: %lu new_size: %lu", (uint64_t) (uint64_t *) dst,
//                    stat->size, asize);
//
//            if (asize > stat->size) {
//                size_t zsize = asize - stat->size;
//                char *oldp = v + stat->size;
//                memset(oldp, 0, zsize);
//            }
//
//            free(stat);
//        }
//    }
//    else
//    {
//        memset(v, 0, asize);
//    }
//
//    _record_alloc_set(v, asize);
    return v;
}

#endif

static void *mmove(void *dst, const void *src, size_t size) {
    return memmove(dst, src, size);
}

static void *mcopy(void *dst, const void *src, size_t size) {
    return memcpy(dst, src, size);
}


//================================================================

//=======================================================================
// CRC64
//=======================================================================

static const uint64_t crc64_tab[256] = {
        UINT64_C(0x0000000000000000), UINT64_C(0x7ad870c830358979),
        UINT64_C(0xf5b0e190606b12f2), UINT64_C(0x8f689158505e9b8b),
        UINT64_C(0xc038e5739841b68f), UINT64_C(0xbae095bba8743ff6),
        UINT64_C(0x358804e3f82aa47d), UINT64_C(0x4f50742bc81f2d04),
        UINT64_C(0xab28ecb46814fe75), UINT64_C(0xd1f09c7c5821770c),
        UINT64_C(0x5e980d24087fec87), UINT64_C(0x24407dec384a65fe),
        UINT64_C(0x6b1009c7f05548fa), UINT64_C(0x11c8790fc060c183),
        UINT64_C(0x9ea0e857903e5a08), UINT64_C(0xe478989fa00bd371),
        UINT64_C(0x7d08ff3b88be6f81), UINT64_C(0x07d08ff3b88be6f8),
        UINT64_C(0x88b81eabe8d57d73), UINT64_C(0xf2606e63d8e0f40a),
        UINT64_C(0xbd301a4810ffd90e), UINT64_C(0xc7e86a8020ca5077),
        UINT64_C(0x4880fbd87094cbfc), UINT64_C(0x32588b1040a14285),
        UINT64_C(0xd620138fe0aa91f4), UINT64_C(0xacf86347d09f188d),
        UINT64_C(0x2390f21f80c18306), UINT64_C(0x594882d7b0f40a7f),
        UINT64_C(0x1618f6fc78eb277b), UINT64_C(0x6cc0863448deae02),
        UINT64_C(0xe3a8176c18803589), UINT64_C(0x997067a428b5bcf0),
        UINT64_C(0xfa11fe77117cdf02), UINT64_C(0x80c98ebf2149567b),
        UINT64_C(0x0fa11fe77117cdf0), UINT64_C(0x75796f2f41224489),
        UINT64_C(0x3a291b04893d698d), UINT64_C(0x40f16bccb908e0f4),
        UINT64_C(0xcf99fa94e9567b7f), UINT64_C(0xb5418a5cd963f206),
        UINT64_C(0x513912c379682177), UINT64_C(0x2be1620b495da80e),
        UINT64_C(0xa489f35319033385), UINT64_C(0xde51839b2936bafc),
        UINT64_C(0x9101f7b0e12997f8), UINT64_C(0xebd98778d11c1e81),
        UINT64_C(0x64b116208142850a), UINT64_C(0x1e6966e8b1770c73),
        UINT64_C(0x8719014c99c2b083), UINT64_C(0xfdc17184a9f739fa),
        UINT64_C(0x72a9e0dcf9a9a271), UINT64_C(0x08719014c99c2b08),
        UINT64_C(0x4721e43f0183060c), UINT64_C(0x3df994f731b68f75),
        UINT64_C(0xb29105af61e814fe), UINT64_C(0xc849756751dd9d87),
        UINT64_C(0x2c31edf8f1d64ef6), UINT64_C(0x56e99d30c1e3c78f),
        UINT64_C(0xd9810c6891bd5c04), UINT64_C(0xa3597ca0a188d57d),
        UINT64_C(0xec09088b6997f879), UINT64_C(0x96d1784359a27100),
        UINT64_C(0x19b9e91b09fcea8b), UINT64_C(0x636199d339c963f2),
        UINT64_C(0xdf7adabd7a6e2d6f), UINT64_C(0xa5a2aa754a5ba416),
        UINT64_C(0x2aca3b2d1a053f9d), UINT64_C(0x50124be52a30b6e4),
        UINT64_C(0x1f423fcee22f9be0), UINT64_C(0x659a4f06d21a1299),
        UINT64_C(0xeaf2de5e82448912), UINT64_C(0x902aae96b271006b),
        UINT64_C(0x74523609127ad31a), UINT64_C(0x0e8a46c1224f5a63),
        UINT64_C(0x81e2d7997211c1e8), UINT64_C(0xfb3aa75142244891),
        UINT64_C(0xb46ad37a8a3b6595), UINT64_C(0xceb2a3b2ba0eecec),
        UINT64_C(0x41da32eaea507767), UINT64_C(0x3b024222da65fe1e),
        UINT64_C(0xa2722586f2d042ee), UINT64_C(0xd8aa554ec2e5cb97),
        UINT64_C(0x57c2c41692bb501c), UINT64_C(0x2d1ab4dea28ed965),
        UINT64_C(0x624ac0f56a91f461), UINT64_C(0x1892b03d5aa47d18),
        UINT64_C(0x97fa21650afae693), UINT64_C(0xed2251ad3acf6fea),
        UINT64_C(0x095ac9329ac4bc9b), UINT64_C(0x7382b9faaaf135e2),
        UINT64_C(0xfcea28a2faafae69), UINT64_C(0x8632586aca9a2710),
        UINT64_C(0xc9622c4102850a14), UINT64_C(0xb3ba5c8932b0836d),
        UINT64_C(0x3cd2cdd162ee18e6), UINT64_C(0x460abd1952db919f),
        UINT64_C(0x256b24ca6b12f26d), UINT64_C(0x5fb354025b277b14),
        UINT64_C(0xd0dbc55a0b79e09f), UINT64_C(0xaa03b5923b4c69e6),
        UINT64_C(0xe553c1b9f35344e2), UINT64_C(0x9f8bb171c366cd9b),
        UINT64_C(0x10e3202993385610), UINT64_C(0x6a3b50e1a30ddf69),
        UINT64_C(0x8e43c87e03060c18), UINT64_C(0xf49bb8b633338561),
        UINT64_C(0x7bf329ee636d1eea), UINT64_C(0x012b592653589793),
        UINT64_C(0x4e7b2d0d9b47ba97), UINT64_C(0x34a35dc5ab7233ee),
        UINT64_C(0xbbcbcc9dfb2ca865), UINT64_C(0xc113bc55cb19211c),
        UINT64_C(0x5863dbf1e3ac9dec), UINT64_C(0x22bbab39d3991495),
        UINT64_C(0xadd33a6183c78f1e), UINT64_C(0xd70b4aa9b3f20667),
        UINT64_C(0x985b3e827bed2b63), UINT64_C(0xe2834e4a4bd8a21a),
        UINT64_C(0x6debdf121b863991), UINT64_C(0x1733afda2bb3b0e8),
        UINT64_C(0xf34b37458bb86399), UINT64_C(0x8993478dbb8deae0),
        UINT64_C(0x06fbd6d5ebd3716b), UINT64_C(0x7c23a61ddbe6f812),
        UINT64_C(0x3373d23613f9d516), UINT64_C(0x49aba2fe23cc5c6f),
        UINT64_C(0xc6c333a67392c7e4), UINT64_C(0xbc1b436e43a74e9d),
        UINT64_C(0x95ac9329ac4bc9b5), UINT64_C(0xef74e3e19c7e40cc),
        UINT64_C(0x601c72b9cc20db47), UINT64_C(0x1ac40271fc15523e),
        UINT64_C(0x5594765a340a7f3a), UINT64_C(0x2f4c0692043ff643),
        UINT64_C(0xa02497ca54616dc8), UINT64_C(0xdafce7026454e4b1),
        UINT64_C(0x3e847f9dc45f37c0), UINT64_C(0x445c0f55f46abeb9),
        UINT64_C(0xcb349e0da4342532), UINT64_C(0xb1eceec59401ac4b),
        UINT64_C(0xfebc9aee5c1e814f), UINT64_C(0x8464ea266c2b0836),
        UINT64_C(0x0b0c7b7e3c7593bd), UINT64_C(0x71d40bb60c401ac4),
        UINT64_C(0xe8a46c1224f5a634), UINT64_C(0x927c1cda14c02f4d),
        UINT64_C(0x1d148d82449eb4c6), UINT64_C(0x67ccfd4a74ab3dbf),
        UINT64_C(0x289c8961bcb410bb), UINT64_C(0x5244f9a98c8199c2),
        UINT64_C(0xdd2c68f1dcdf0249), UINT64_C(0xa7f41839ecea8b30),
        UINT64_C(0x438c80a64ce15841), UINT64_C(0x3954f06e7cd4d138),
        UINT64_C(0xb63c61362c8a4ab3), UINT64_C(0xcce411fe1cbfc3ca),
        UINT64_C(0x83b465d5d4a0eece), UINT64_C(0xf96c151de49567b7),
        UINT64_C(0x76048445b4cbfc3c), UINT64_C(0x0cdcf48d84fe7545),
        UINT64_C(0x6fbd6d5ebd3716b7), UINT64_C(0x15651d968d029fce),
        UINT64_C(0x9a0d8ccedd5c0445), UINT64_C(0xe0d5fc06ed698d3c),
        UINT64_C(0xaf85882d2576a038), UINT64_C(0xd55df8e515432941),
        UINT64_C(0x5a3569bd451db2ca), UINT64_C(0x20ed197575283bb3),
        UINT64_C(0xc49581ead523e8c2), UINT64_C(0xbe4df122e51661bb),
        UINT64_C(0x3125607ab548fa30), UINT64_C(0x4bfd10b2857d7349),
        UINT64_C(0x04ad64994d625e4d), UINT64_C(0x7e7514517d57d734),
        UINT64_C(0xf11d85092d094cbf), UINT64_C(0x8bc5f5c11d3cc5c6),
        UINT64_C(0x12b5926535897936), UINT64_C(0x686de2ad05bcf04f),
        UINT64_C(0xe70573f555e26bc4), UINT64_C(0x9ddd033d65d7e2bd),
        UINT64_C(0xd28d7716adc8cfb9), UINT64_C(0xa85507de9dfd46c0),
        UINT64_C(0x273d9686cda3dd4b), UINT64_C(0x5de5e64efd965432),
        UINT64_C(0xb99d7ed15d9d8743), UINT64_C(0xc3450e196da80e3a),
        UINT64_C(0x4c2d9f413df695b1), UINT64_C(0x36f5ef890dc31cc8),
        UINT64_C(0x79a59ba2c5dc31cc), UINT64_C(0x037deb6af5e9b8b5),
        UINT64_C(0x8c157a32a5b7233e), UINT64_C(0xf6cd0afa9582aa47),
        UINT64_C(0x4ad64994d625e4da), UINT64_C(0x300e395ce6106da3),
        UINT64_C(0xbf66a804b64ef628), UINT64_C(0xc5bed8cc867b7f51),
        UINT64_C(0x8aeeace74e645255), UINT64_C(0xf036dc2f7e51db2c),
        UINT64_C(0x7f5e4d772e0f40a7), UINT64_C(0x05863dbf1e3ac9de),
        UINT64_C(0xe1fea520be311aaf), UINT64_C(0x9b26d5e88e0493d6),
        UINT64_C(0x144e44b0de5a085d), UINT64_C(0x6e963478ee6f8124),
        UINT64_C(0x21c640532670ac20), UINT64_C(0x5b1e309b16452559),
        UINT64_C(0xd476a1c3461bbed2), UINT64_C(0xaeaed10b762e37ab),
        UINT64_C(0x37deb6af5e9b8b5b), UINT64_C(0x4d06c6676eae0222),
        UINT64_C(0xc26e573f3ef099a9), UINT64_C(0xb8b627f70ec510d0),
        UINT64_C(0xf7e653dcc6da3dd4), UINT64_C(0x8d3e2314f6efb4ad),
        UINT64_C(0x0256b24ca6b12f26), UINT64_C(0x788ec2849684a65f),
        UINT64_C(0x9cf65a1b368f752e), UINT64_C(0xe62e2ad306bafc57),
        UINT64_C(0x6946bb8b56e467dc), UINT64_C(0x139ecb4366d1eea5),
        UINT64_C(0x5ccebf68aecec3a1), UINT64_C(0x2616cfa09efb4ad8),
        UINT64_C(0xa97e5ef8cea5d153), UINT64_C(0xd3a62e30fe90582a),
        UINT64_C(0xb0c7b7e3c7593bd8), UINT64_C(0xca1fc72bf76cb2a1),
        UINT64_C(0x45775673a732292a), UINT64_C(0x3faf26bb9707a053),
        UINT64_C(0x70ff52905f188d57), UINT64_C(0x0a2722586f2d042e),
        UINT64_C(0x854fb3003f739fa5), UINT64_C(0xff97c3c80f4616dc),
        UINT64_C(0x1bef5b57af4dc5ad), UINT64_C(0x61372b9f9f784cd4),
        UINT64_C(0xee5fbac7cf26d75f), UINT64_C(0x9487ca0fff135e26),
        UINT64_C(0xdbd7be24370c7322), UINT64_C(0xa10fceec0739fa5b),
        UINT64_C(0x2e675fb4576761d0), UINT64_C(0x54bf2f7c6752e8a9),
        UINT64_C(0xcdcf48d84fe75459), UINT64_C(0xb71738107fd2dd20),
        UINT64_C(0x387fa9482f8c46ab), UINT64_C(0x42a7d9801fb9cfd2),
        UINT64_C(0x0df7adabd7a6e2d6), UINT64_C(0x772fdd63e7936baf),
        UINT64_C(0xf8474c3bb7cdf024), UINT64_C(0x829f3cf387f8795d),
        UINT64_C(0x66e7a46c27f3aa2c), UINT64_C(0x1c3fd4a417c62355),
        UINT64_C(0x935745fc4798b8de), UINT64_C(0xe98f353477ad31a7),
        UINT64_C(0xa6df411fbfb21ca3), UINT64_C(0xdc0731d78f8795da),
        UINT64_C(0x536fa08fdfd90e51), UINT64_C(0x29b7d047efec8728),
};

static uint64_t crc64(uint64_t crc, const unsigned char *s, uint64_t l) {
    uint64_t j;

    for (j = 0; j < l; j++) {
        uint8_t byte = s[j];
        crc = crc64_tab[(uint8_t) crc ^ byte] ^ (crc >> 8);
    }
    return crc;
}


static uint64_t crc64s(const char *str) {
    uint64_t l = strlen(str);
    return crc64(0, (const unsigned char *) str, l);
}

//=======================================================================
// HASH TABLE
//=======================================================================

static unsigned long ht_hash(const char *_str) {
    const unsigned char *str = (const unsigned char *) _str;
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


static ret_t ht_init(hashtable_t **ht) {
    *ht = calloc(sizeof(hashtable_t), 1);
    return ST_OK;
}

static void ht_destroy_item(ht_item_t *item) {
    free(item->key);
    free(item->value->ptr);
    free(item->value);
    free(item);
}

static void ht_destroy_items_line(ht_item_t *start_item) {
    ht_item_t *next = start_item;
    ht_item_t *tmp = NULL;
    while (next) {
        tmp = next;
        next = next->next;

        ht_destroy_item(tmp);
    }
}


static void ht_destroy(hashtable_t *ht) {
    for (size_t i = 0; i < HASHTABLE_SIZE; ++i) {
        ht_destroy_items_line(ht->table[i]);
    }

    free(ht);
}


static ret_t ht_create_item(ht_item_t **pitem, uint64_t name_hash, ht_value_t *value) {
    ht_item_t *item;
    if ((item = calloc(sizeof(ht_item_t), 1)) == NULL) {
        LOG_ERROR(" can't alloc");
        return ST_ERR;
    }

    item->hash = name_hash;
    item->value = value;

    *pitem = item;

    return ST_OK;
}


static ret_t ht_set(hashtable_t *ht, ht_key_t *key, ht_value_t *value) {
    uint64_t hash = key->u.i;
    size_t bin = hash % HASHTABLE_SIZE;

    ht_item_t *item = ht->table[bin];
    ht_item_t *prev = NULL;
    while (item) {
        if (item->hash == hash)
            break;

        prev = item;
        item = item->next;
    }


    if (item && item->hash == hash) {
        item->value = value;
        free(key);
    } else {
        ht_item_t *new_item = NULL;
        if ((ht_create_item(&new_item, hash, value)) != ST_OK) {
            return ST_ERR;
        }

        new_item->key = key;

        if (prev)
            prev->next = new_item;
        else
            ht->table[bin] = new_item;
    }

    return ST_OK;
}

static ret_t ht_get(hashtable_t *ht, ht_key_t *key, ht_value_t **value) {
    uint64_t hash = key->u.i;
    uint64_t bin = hash % HASHTABLE_SIZE;

    ht_item_t *item = ht->table[bin];

    while (item) {
        if (item->hash == hash) {
            *value = item->value;
            return ST_OK;
        }

        item = item->next;
    }

    return ST_NOT_FOUND;
}

static ret_t ht_foreach(hashtable_t *ht, ht_forach_cb cb) {
    for (size_t i = 0; i < HASHTABLE_SIZE; ++i) {
        ht_item_t *next = ht->table[i];
        while (next) {
            cb(next->hash, next->key, next->value);
            next = next->next;
        }
    }

    return ST_OK;
}

//=======================================================================
// DYNAMIC ALLOCATOR
//=======================================================================

#define DA_TRACE(a) (LOG_TRACE("[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu", \
a, a->ptr, a->size, a->used, a->mul))

typedef struct dynamic_allocator {
    char *ptr;
    size_t size;
    size_t used;
    size_t mul;

} dynamic_allocator_t;

static ret_t da_init(dynamic_allocator_t **a);

static ret_t da_realloc(dynamic_allocator_t *a, size_t size) {
    if (a == NULL) {
        LOG_WARN("Empty dynamic_allocator::ptr");
        DA_TRACE(a);

        return da_init(&a);
    }

    if (a->size == size) {
        return ST_OK;
    } else if (size < a->size || size == 0) {
        size_t ds = a->size - size;
        memset(a->ptr + size, 0, ds);
        a->ptr = realloc(a->ptr, size);
        a->size = size;
        a->used = size;
        a->mul = 1;
    } else if (size > a->size) {
        size_t ds = size - a->size;
        a->ptr = realloc(a->ptr, size);
        memset(a->ptr + a->size, 0, ds);
        a->size = size;
    }

    return return_create(ST_OK);
}

static ret_t da_init_n(dynamic_allocator_t **a, size_t size) {
    *a = zalloc(sizeof(dynamic_allocator_t));

    (*a)->ptr = zalloc(size);
    (*a)->size = size;
    (*a)->mul = 1;

    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              (*a), (*a)->ptr, (*a)->size, (*a)->used, (*a)->mul);

    return return_create(ST_OK);
}

static ret_t da_init(dynamic_allocator_t **a) {

    return da_init_n(a, STRING_INIT_BUFFER);
}

static ret_t da_release(dynamic_allocator_t *a) {
    if (!a)
        return return_create(ST_EMPTY);

    if (a) {
        LOG_TRACE("a[0x%08lX] size=%lu used=%lu mul=%lu",
                  a->ptr, a->size, a->used, a->mul);

        safe_free(a->ptr);
        safe_free(a);
    }

    return return_create(ST_OK);;
}

static ret_t da_fit(dynamic_allocator_t *a) {

    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);


    da_realloc(a, a->used);


    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return return_create(ST_OK);;
}

static ret_t da_crop_tail(dynamic_allocator_t *a, size_t n) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
              a, a->ptr, a->size, a->used, a->mul, n);

    if (n > a->size) {
        LOG_WARN("a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
                 a, a->ptr, a->size, a->used, a->mul, n);

        return return_create(ST_OUT_OF_RANGE);
    }

    size_t nsize = a->used - n;
    char *newbuff = zalloc(nsize);
    memcpy(newbuff, a->ptr + n, nsize);
    free(a->ptr);
    a->ptr = newbuff;
    a->size = nsize;
    a->used = a->size;

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return return_create(ST_OK);;
}

static ret_t da_pop_head(dynamic_allocator_t *a, size_t n) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; n=%lu",
              a, a->ptr, a->size, a->used, a->mul, n);

    da_fit(a);
    if (n > a->size) {
        LOG_WARN("a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
                 a, a->ptr, a->size, a->used, a->mul, n);

        return return_create(ST_OUT_OF_RANGE);
    }

    da_realloc(a, a->size - n);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return return_create(ST_OK);
}

static ret_t da_check_size(dynamic_allocator_t *a, size_t new_size) {
    if (a->size < a->used + new_size)
        da_realloc(a, a->used + new_size);

    return return_create(ST_OK);
}

static ret_t da_append(dynamic_allocator_t *a, const char *data, size_t size) {

    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    LOG_TRACE("input data=0x%08lX size=%lu", data, size);

    da_check_size(a, size);

    mcopy(a->ptr + a->used, data, size);
    a->used += size;


    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return return_create(ST_OK);
}

static ret_t da_sub(dynamic_allocator_t *a, size_t begin, size_t end, dynamic_allocator_t **b) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; input begin=%lu end=%lu",
              a, a->ptr, a->size, a->used, a->mul, begin, end);

    size_t ssize = end - begin;
    if (ssize > a->used) {

        LOG_WARN("a=[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; begin=%lu and end=%lu out of range",
                 a, a->ptr, a->size, a->used, a->mul, begin, end);

        return return_create(ST_OUT_OF_RANGE);
    }

    da_init_n(b, ssize);
    da_append(*b, a->ptr + begin, ssize);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    LOG_TRACE("e b[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              (*b), (*b)->ptr, (*b)->size, (*b)->used, (*b)->mul);

    return return_create(ST_OK);

}


static ret_t da_dub(dynamic_allocator_t *a, dynamic_allocator_t **b) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    size_t b_size = a->used;
    da_init_n(b, b_size);
    da_append(*b, a->ptr, b_size);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              (*b), (*b)->ptr, (*b)->size, (*b)->used, (*b)->mul);

    return return_create(ST_OK);
}

static ret_t da_merge(dynamic_allocator_t *a, dynamic_allocator_t **b) {
    DA_TRACE(a);
    DA_TRACE((*b));

    da_fit(a);
    da_fit(*b);

    size_t nb_size = a->size + (*b)->size;

    da_realloc(a, nb_size);

    mcopy(a->ptr + a->used, (*b)->ptr, (*b)->size);

    a->used += (*b)->size;

    da_release(*b);
    *b = NULL;

    DA_TRACE(a);
    ASSERT(*b == NULL);

    return return_create(ST_OK);
}

static ret_t da_concant(dynamic_allocator_t *a, dynamic_allocator_t *b) {
    DA_TRACE(a);
    DA_TRACE(b);

    da_fit(a);
    da_fit(b);

    size_t nb_size = a->size + b->size;

    da_realloc(a, nb_size);

    mcopy(a->ptr + a->used, b->ptr, b->size);

    a->used += b->size;


    DA_TRACE(a);
    DA_TRACE(b);

    return return_create(ST_OK);
}

static ret_t da_remove(dynamic_allocator_t *a, size_t begin, size_t end) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; begin=%lu end=%lu",
              a, a->ptr, a->size, a->used, a->mul, begin, end);

    size_t dsize = end - begin;
    if (dsize > a->used) {
        LOG_WARN("begin(%lu) + end(%lu) >= total used(%lu) bytes", begin, end, a->used);
        return return_create(ST_SIZE_EXCEED);
    }

    dynamic_allocator_t *b = NULL;

    da_sub(a, end, a->used, &b);

    da_realloc(a, begin);

    da_merge(a, &b);

    ASSERT(b == NULL);
    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return return_create(ST_OK);

}

static ret_t da_compare(dynamic_allocator_t *a, dynamic_allocator_t *b) {
    if (a->used < b->used)
        return return_create2(ST_NOT_FOUND, 1);
    else if (a->used > b->used)
        return return_create2(ST_NOT_FOUND, (uint16_t) -1);
    else
        return return_create2(ST_OK, memcmp(a->ptr, b->ptr, MIN(a->used, b->used)));
}


static ret_t da_comparez(dynamic_allocator_t *a, const char *b) {
    size_t ssize = strlen(b);
    return return_create2(ST_OK, memcmp(a->ptr, b, MIN(a->used, ssize)));
}

static void test_da() {
    ret_t ret = 0;
    dynamic_allocator_t *da = NULL;
    dynamic_allocator_t *da2 = NULL;
    dynamic_allocator_t *db = NULL;
    dynamic_allocator_t *dc = NULL;
    dynamic_allocator_t *dd = NULL;
    dynamic_allocator_t *de = NULL;
    dynamic_allocator_t *df = NULL;

    CHECK_RETURN(da_init(&da));

    // 0    H
    // 1    e
    // 2    l
    // 3    l
    // 4    o
    // 5    ,
    // 6
    // 7    S
    // 8    w
    // 9    e
    // 10   e
    // 11   t
    // 12
    // 13   M
    // 14   a
    // 15   r
    // 16   i
    // 17   a
    // 18   !
    // 19   EOF

#define STR_TO_PZ(str) str, strlen(str)

//    char const *str1 = "Hello, Sweet Maria!";
    CHECK_RETURN(da_append(da, STR_TO_PZ("Hello, ")));
    CHECK_RETURN(da_append(da, STR_TO_PZ("Sweet Maria!")));

#undef STR_TO_PZ

    CHECK_RETURN(da_dub(da, &da2));
    CHECK_RETURN(da_remove(da2, 0, 7));

    ret = da_comparez(da2, "Sweet Maria!");
    ASSERT(return_code(ret) == ST_OK && return_data(ret) == 0);

    CHECK_RETURN(da_sub(da, 13, 19, &db));

    ret = da_comparez(db, "Maria!");
    ASSERT(return_code(ret) == ST_OK && return_data(ret) == 0);

    ret = da_compare(db, db);
    ASSERT(return_code(ret) == ST_OK && return_data(ret) == 0);

    CHECK_RETURN(da_init(&de));
    CHECK_RETURN(da_append(de, " My ", 4));

    ret = da_comparez(de, " My ");
    ASSERT(return_code(ret) == ST_OK && return_data(ret) == 0);

    CHECK_RETURN(da_sub(da, 7, 12, &df));

    ret = da_comparez(df, "Sweet");
    ASSERT(return_code(ret) == ST_OK && return_data(ret) == 0);


    CHECK_RETURN(da_append(df, "!", 1));

    CHECK_RETURN(da_remove(da, 7, 13));

    ret = da_comparez(da, "Hello, Maria!");
    ASSERT(return_code(ret) == ST_OK && return_data(ret) == 0);

    CHECK_RETURN(da_merge(de, &df));

    CHECK_RETURN(da_concant(da, de));


    da_release(da);
    da = NULL;

    da_release(da2);
    da2 = NULL;

    da_release(db);
    db = NULL;

    da_release(dc);
    dc = NULL;

    da_release(dd);
    dd = NULL;

    da_release(de);
    de = NULL;

    da_release(df);
    df = NULL;
}

//=======================================================================
// REGEX
//=======================================================================

#define MAX_ERROR_MSG 0x1000

/* Compile the regular expression described by "regex_text" into
   "r". */

static ret_t regex_compile(regex_t *r, const char *pattern) {
    int status = regcomp(r, pattern, REG_EXTENDED | REG_NEWLINE);
    if (status != 0) {
        char error_message[MAX_ERROR_MSG];
        regerror(status, r, error_message, MAX_ERROR_MSG);

        LOG_ERROR("Regex error compiling '%s': %s",
                  pattern, error_message);
        return return_create(ST_ERR);
    }
    return ST_OK;
}

static bool regex_match(regex_t *r, const char *text) {
    regmatch_t m[10];
    int nomatch = regexec(r, text, 10, m, 0);
    if (nomatch) {
        return false;
    }

    return true;
}

//=======================================================================
// STRING
//=======================================================================

typedef struct {
    union {
        uint8_t _[sizeof(dynamic_allocator_t)];
        dynamic_allocator_t da;
    };

} string;


#define _da(x) ((dynamic_allocator_t*)x)
#define _dap(x) ((dynamic_allocator_t**)x)

typedef struct skey_value {
    string *key;
    string *value;
} skey_value_t;

static string *string_null = NULL;

static ret_t string_init(string **sp) {
    return da_init(_dap(sp));
}

static ret_t string_release(string *s) {
    return da_release(_da(s));
}

static void string_release_cb(void *p) {
    string_release((string *) p);
}

static const char *string_cdata(string *s) {
    return _da(s)->ptr;
}

static size_t string_size(string *s) {
    return _da(s)->used;
}

static char string_char(string *s, size_t idx) {
    if (idx > string_size(s)) {
        LOG_ERROR("Index is out of bound %lu > %lu", idx, string_size(s));
        return (char) -1;
    }

    return _da(s)->ptr[idx];
}

static ret_t string_create_nt(string *s, char **buff) {
    size_t ssize = string_size(s);
    *buff = zalloc(ssize + 1);
    memcpy(*buff, string_cdata(s), ssize);

    return ST_OK;
}

///deep copy
static ret_t string_dub(string *s, string **ns) {
    return da_dub(_da(s), _dap(ns));
}

static ret_t string_append(string *s, const char *str) {
    size_t len = strlen(str);
    return da_append(_da(s), str, len);

}

static ret_t string_appendf(string *s, const char *fmt, ...) {
    static const size_t FORMAT_BUFFER_SIZE = 4096;
    va_list args;
    va_start(args, fmt);

    char buf[FORMAT_BUFFER_SIZE];
    memset(buf, 0, FORMAT_BUFFER_SIZE);
    vsnprintf(buf, FORMAT_BUFFER_SIZE, fmt, args);

    va_end(args);

    return string_append(s, buf);
}

static ret_t string_append_se(string *s, const char *start, const char *end) {
    size_t sz = end - start;
    return da_append(_da(s), start, sz);
}

static void string_init_globals() {
    string_init(&string_null);
    string_append(string_null, "(null)");
}

static void string_shutdown_globals() {
    if (string_null)
        string_release(string_null);
}

static char *string_makez(string *s) {
    if (!s) return NULL;

    size_t sz = _da(s)->used;
    char *data = zalloc(sz + 1);
    memcpy(data, _da(s)->ptr, sz);
    return data;
}

static ret_t string_appendn(string *s, const char *str, size_t len) {
    return da_append(_da(s), str, len);

}

static ret_t string_create(string **s, const char *str) {
    string_init(s);

    return string_append(*s, str);
}

static ret_t string_add(string *a, string *b) {
    return da_concant(_da(a), _da(b));
}

static ret_t string_pop_head(string *s, size_t n) {
    return da_pop_head(_da(s), n);
}

static ret_t string_crop_tail(string *s, size_t n) {
    return da_crop_tail(_da(s), n);
}

static size_t string_find_last_char(string *s, char ch) {
    size_t ssize = string_size(s);
    for (size_t i = ssize; i != 0; --i) {
        char cur = string_char(s, i);
        if (cur == ch)
            return i;
    }

    return size_npos;

}

static ret_t string_remove_seq(string *s, size_t begin, size_t end) {
    return da_remove(_da(s), begin, end);
}

static ret_t string_remove_dubseq(string *s, char delm, uint8_t skip) {
    size_t j = 0;
    while (j < string_size(s)) {
        const char *cur = string_cdata(s);

        if (cur[j] == delm) {
            size_t begin = j;
            size_t end = begin;

            while (cur[(++end)] == delm);

            //--end;// last compared position
            size_t n = end - begin;

            if (n > 1) {

                size_t skip_ = 0;
                if (skip >= n) {
                    LOG_WARN("skip value is too high. skip=%d", skip);
                } else {
                    skip_ = (size_t) skip;
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

static bool check_strip_dict(char ch) {
    for (size_t i = 0; i < sizeof(strip_dict) / sizeof(char); ++i)
        if (ch == strip_dict[i])
            return true;

    return false;
}

static ret_t string_rstrip(string *s) {

    if (string_size(s) < 1) {
        LOG_WARN("String size(%lu) is too small", string_size(s));
        return return_create(ST_OUT_OF_RANGE);
    }

    size_t n = 0;
    for (size_t j = string_size(s) - 1; j != 0; --j) {
        char sch = string_char(s, j);
        if (check_strip_dict(sch))
            ++n;
        else
            break;
    }

    return string_pop_head(s, n);
}

static ret_t string_lstrip(string *s) {

    size_t n = 0;
    for (size_t j = 0; j < string_size(s); ++j) {
        char sch = string_char(s, j);
        if (check_strip_dict(sch))
            ++n;
        else
            break;
    }

    return string_crop_tail(s, n);
}

static ret_t string_strip(string *s) {

    if (string_size(s) < 3)
        return ST_SIZE_EXCEED;

    string_rstrip(s);
    string_lstrip(s);

    return ST_OK;
}

static bool check_strip_dict_ws(char ch) {
    for (size_t i = 0; i < sizeof(strip_dict_ws) / sizeof(char); ++i)
        if (ch == strip_dict_ws[i])
            return true;

    return false;
}

static ret_t string_rstrip_ws(string *s) {
    while (string_size(s) > 2) {

        size_t idx = string_size(s) - 1;
        char sch = string_char(s, idx);

        if (check_strip_dict_ws(sch)) {
            string_pop_head(s, 1);
        } else {
            return ST_OK;
        }

    }

    return ST_OK;
}

static ret_t string_starts_with(string *s, const char *str) {
    size_t str_len = strlen(str);
    if (str_len > string_size(s))
        return return_create(ST_SIZE_EXCEED);

    if (memcmp(string_cdata(s), str, str_len) == 0)
        return return_create(ST_OK);

    return return_create(ST_NOT_FOUND);
}


static bool string_re_match(string *s, const char *pattern) {
    regex_t re;
    regex_compile(&re, pattern);

    char *text = string_makez(s);
    bool m = regex_match(&re, text);
    free(text);
    regfree(&re);

    return m;
}

static ret_t string_compare(string *a, string *b) {
    return da_compare(_da(a), _da(b));
}

static ret_t string_comparez(string *a, const char *str) {
    size_t ssize = strlen(str);
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

static ret_t string_map_region(string *s, size_t beg, size_t end, char **sb, char **se) {
    size_t ssize = string_size(s);
    if ((beg > 0 && beg <= ssize) && (end > 0 && end <= ssize)) {
        LOG_ERROR("Indexes are out of bound");
        return ST_OUT_OF_RANGE;
    }

    *sb = _da(s)->ptr + beg;
    *se = _da(s)->ptr + end;

    return ST_OK;
}

static void string_map_string(string *s, char **sb, char **se) {
    string_map_region(s, 0, string_size(s), sb, se);
}

static ret_t string_to_u64(string *s, uint64_t *ul) {

    uint64_t res = 0;
    size_t ssize = string_size(s);
    for (size_t i = 0; i < ssize; ++i)
        res = res * 10 + string_char(s, i) - '0';

    *ul = res;

    return ST_OK;

}

#define string_print(a) LOG_INFO("%.*s", string_size(a), string_cdata(a));
#define string_printd(a) LOG_DEBUG("%.*s", string_size(a), string_cdata(a));
#define string_printt(a) LOG_TRACE("%.*s", string_size(a), string_cdata(a));

static void test_string() {
    static const char *str1 = "Hello, World!";
    static const char *str2 = "What's up, Dude?";
    ASSERT(sizeof(dynamic_allocator_t) == sizeof(string));

    string *a = NULL;
    string *b = NULL;
    string *c = NULL;
    string *d = NULL;


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

    ASSERT(string_comparez(d, "b3_2_1") == RET_OK);
    string_print(d);


    string_release(a);
    string_release(b);
    string_release(c);
    string_release(d);

}

//=======================================================================
// GENERIC FIFO
//=======================================================================

typedef struct fifo_node {
    void *data;
    struct fifo_node *next;

} fifo_node_t;

typedef struct fifo {
    fifo_node_t *head;
    fifo_node_t *top;
    size_t size;
    data_release_cb rel_cb;

} fifo_t;


static void fifo_init(fifo_t **l, data_release_cb cb) {
    *l = zalloc(sizeof(fifo_t));
    (*l)->rel_cb = cb;
}

static void fifo_node_init(fifo_node_t **node) {
    *node = zalloc(sizeof(fifo_node_t));
}

static void fifo_push(fifo_t *l, void *s) {

    if (!l->head) {
        fifo_node_init(&l->head);

        fifo_node_t *node = l->head;

        node->data = s;

        l->top = l->head;
    } else {
        fifo_node_t *node = NULL;
        fifo_node_init(&node);
        node->data = s;
        l->top->next = node;


        l->top = node;

    }

    ++l->size;
}

static void *fifo_pop(fifo_t *l) {
    if (!l->head)
        return NULL;

    fifo_node_t *tmp = l->head;
    l->head = tmp->next;
    l->size--;

    void *p = tmp->data;
    safe_free(tmp);

    return p;
}

static ret_t fifo_release(fifo_t *l, bool data_release) {

    void *p = NULL;
    while ((p = fifo_pop(l))) {
        if (data_release)
            l->rel_cb(p);
    }

    safe_free(l);


    return RET_OK;

}

void test_fifo() {
    static uint64_t ii[] = {0, 1, 2, 3, 4, 5, 6, 7};


    fifo_t *f = NULL;
    fifo_init(&f, NULL);

    fifo_push(f, &ii[0]);
    fifo_push(f, &ii[1]);
    fifo_push(f, &ii[2]);
    fifo_push(f, &ii[3]);
    fifo_push(f, &ii[4]);
    fifo_push(f, &ii[5]);
    fifo_push(f, &ii[6]);
    fifo_push(f, &ii[7]);


    uint64_t *p = NULL;
    uint64_t i = 0;
    while ((p = (uint64_t *) fifo_pop(f))) {
        ASSERT(ii[i++] == *p);
        LOG_TRACE("fifo %lu", *p);
    }


    fifo_release(f, false);

}

//=======================================================================
// GENERIC LIFO
//=======================================================================

typedef struct lifo_node {
    void *data;
    struct lifo_node *prev;

} lifo_node_t;

typedef struct lifo {
    lifo_node_t *head;
    size_t size;
    data_release_cb rel_cb;

} lifo_t;


static void lifo_init(lifo_t **l, data_release_cb cb) {
    *l = zalloc(sizeof(lifo_t));
    (*l)->rel_cb = cb;
}

static void lifo_node_init(lifo_node_t **node) {
    *node = zalloc(sizeof(lifo_node_t));
}

static void lifo_push(lifo_t *l, void *s) {

    if (!l->head) {
        lifo_node_init(&l->head);

        lifo_node_t *node = l->head;

        node->data = s;
    } else {
        lifo_node_t *node = NULL;
        lifo_node_init(&node);
        node->data = s;
        node->prev = l->head;


        l->head = node;
    }

    ++l->size;
}

static void *lifo_pop(lifo_t *l) {
    if (!l->head)
        return NULL;

    lifo_node_t *tmp = l->head;
    l->head = tmp->prev;
    l->size--;

    void *p = tmp->data;
    safe_free(tmp);

    return p;
}

static ret_t lifo_release(lifo_t *l, bool data_release) {

    void *p = NULL;
    while ((p = lifo_pop(l))) {
        if (data_release)
            l->rel_cb(p);
    }

    safe_free(l);


    return RET_OK;

}

void test_lifo() {
    static uint64_t ii[] = {0, 1, 2, 3, 4, 5, 6, 7};


    lifo_t *f = NULL;
    lifo_init(&f, NULL);

    lifo_push(f, &ii[0]);
    lifo_push(f, &ii[1]);
    lifo_push(f, &ii[2]);
    lifo_push(f, &ii[3]);
    lifo_push(f, &ii[4]);
    lifo_push(f, &ii[5]);
    lifo_push(f, &ii[6]);
    lifo_push(f, &ii[7]);


    uint64_t *p = NULL;
    uint64_t i = 7;
    while ((p = (uint64_t *) lifo_pop(f))) {
        ASSERT(ii[i--] == *p);
        LOG_TRACE("lifo %lu", *p);
    }


    lifo_release(f, false);

}


//=======================================================================
// GENERIC DOUBLE LINKED LIST
//=======================================================================

typedef struct list_node {
    void *data;
    struct list_node *prev;
    struct list_node *next;

} list_node_t;

typedef struct list {
    list_node_t *head;
    list_node_t *tail;
    size_t size;
    data_release_cb rel_cb;

} list_t;

typedef struct list_iter {
    list_node_t *node;
} list_iter_t;


static void list_iter_init(list_t *l, list_iter_t **it) {
    *it = zalloc(sizeof(list_iter_t));
    (*it)->node = l->head;
}

static void list_iter_release(list_iter_t *it) {
    safe_free(it);
}

static void *list_iter_next(list_iter_t *it) {
    if (it->node == NULL)
        return NULL;

    void *p = it->node->data;
    it->node = it->node->next;

    return p;
}


static void list_init(list_t **l, data_release_cb cb) {
    *l = zalloc(sizeof(list_t));
    (*l)->rel_cb = cb;
}

static void list_node_init(list_node_t **node) {
    *node = zalloc(sizeof(list_node_t));
}

static void list_push(list_t *l, void *s) {

    if (!l->head) {
        list_node_init(&l->head);

        list_node_t *node = l->head;

        node->data = s;

        l->tail = l->head;
    } else {
        list_node_t *node = NULL;
        list_node_init(&node);
        node->data = s;

        list_node_t *tail = l->head;
        while (tail->next) {
            tail = tail->next;
        }

        tail->next = node;

        node->prev = tail;


        l->tail = node;

    }

    ++l->size;
}

static void *list_pop_head(list_t *l) {
    if (l->size == 0)
        return NULL;

    list_node_t *tmp = l->head;
    l->head = tmp->next;
    l->size--;

    void *data = tmp->data;
    free(tmp);

    return data;
}

static void *list_crop_tail(list_t *l) {
    if (l->size == 0)
        return NULL;

    list_node_t *tmp = l->tail;
    l->tail = tmp->prev;
    l->size--;

    void *data = tmp->data;
    free(tmp);

    return data;
}

static ret_t list_release(list_t *l, bool release_data) {
    if (!l)
        return ST_EMPTY;


    list_node_t *head = l->head;
    while (head) {

        list_node_t *tmp = head;

        if (release_data)
            l->rel_cb(tmp->data);


        head = head->next;

        free(tmp);
    }

    free(l);

    return ST_OK;

}

static ret_t list_merge(list_t *a, list_t *b) {
    if (a->size == 0 && b->size == 0)
        return ST_ERR;


    a->tail->next = b->head;
    b->head->prev = a->tail;
    a->size += b->size;
    a->tail = b->tail;

    return ST_OK;
}

//TODO segfault
static ret_t list_remove(list_t *l, const void *data) {
    list_node_t *head = l->head;

    while (head) {
        if (head->data == data) {
            list_node_t *hn = head->next;
            list_node_t *hp = head->prev;

            hn->prev->next = hp;
            hp->next->prev = hn;

            free(head);
            --l->size;

            return RET_OK;

        }

        head = head->next;
    }

    return return_create(ST_NOT_FOUND);
}

typedef void(*list_traverse_cb)(list_node_t *);


static ret_t list_traverse(list_t *l, bool forward, list_traverse_cb cb) {
    if (!l) return ST_EMPTY;


    list_node_t *cur = forward ? l->tail : l->head;

    while (cur) {
        cb(cur);
        cur = forward ? cur->prev : cur->next;
    }

    return ST_OK;
}

void test_list_traverse(list_node_t *node) {
    uint64_t i = *((uint64_t *) node->data);
    LOG_TRACE("list elem %lu", i);
}

void test_list() {
    static uint64_t ii[] = {0, 1, 2, 3, 4, 5, 6, 7};
    list_t *l = NULL;
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

    list_node_t *head = l->tail;
    size_t i = 7;
    while (head) {
        uint64_t *pi = (uint64_t *) head->data;

        LOG_TRACE("list elem i[%lu] = %lu", i, *pi);
        ASSERT(ii[i] == *pi);

        --i;
        head = head->prev;
    }

    i = 0;
    uint64_t *pdata = NULL;
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
        uint64_t *pi = (uint64_t *) head->data;

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
//    list_init(&l, sizeof(uint64_t));
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
//        uint64_t* pi = (uint64_t*)head->data;
//
//        LOG_TRACE("list elem i[%lu] = %lu", i, *pi);
//        ASSERT((*pi % 2) == 0);
//
//        head = head->next;
//    }


    list_t *l2 = NULL;
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
        uint64_t *pi = (uint64_t *) head->data;

        LOG_TRACE("list elem i[%lu] = %lu", i, *pi);
        ASSERT(ii[i] == *pi);

        ++i;
        head = head->next;
    }

    head = l->tail;
    i = 7;
    while (head) {
        uint64_t *pi = (uint64_t *) head->data;

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


//=======================================================================
// SLIST
//=======================================================================

static void slist_fprint(list_t *sl) {

    if (!sl) {
        LOG_WARN("sl == NULL");
        return;
    }

    list_node_t *head = sl->head;
    while (head) {
        string *s = (string *) head->data;
#ifdef NDEBUG
        string_print(s);
#else
        string_printd(s);
#endif

        head = head->next;
    }
}

static void slist_rfprint(list_t *sl) {
    if (!sl) {
        LOG_WARN("sl == NULL");
        return;
    }


    list_node_t *tail = sl->tail;

    while (tail) {
        string *s = (string *) tail->data;
#ifdef NDEBUG
        string_print(s);
#else
        string_printd(s);
#endif
        tail = tail->prev;
    }
}

string *slist_next(list_iter_t *it) {
    return (string *) list_iter_next(it);
}

static ret_t string_split(string *s, char delm, list_t **l) {
    if (string_size(s) == 0)
        return ST_EMPTY;


    string_rstrip_ws(s);
    string_remove_dubseq(s, delm, 1);

    list_init(l, &string_release_cb);
    char *cb = NULL;
    char *end = NULL;


    string_map_string(s, &cb, &end);
    char *ccur = cb;

    while (ccur <= end) {
        if (*ccur == delm || ccur == end) {
            //while(*(++ccur) == delm && ccur == end);


            string *ss = NULL;
            string_init(&ss);
            string_appendn(ss, cb, (size_t) (ccur - cb));

            list_push(*l, ss);

            cb = ++ccur;
        }

        ++ccur;
    }

    return ST_OK;
}


//=======================================================================
// GENERIC VECTOR
//=======================================================================
typedef struct vector {
    dynamic_allocator_t *alloc;
    size_t size;
    size_t elem_size;
} vector_t;

static ret_t vector_init(vector_t **vec, size_t elem_size) {
    *vec = zalloc(sizeof(vector_t));
    da_init_n(&(*vec)->alloc, elem_size * 10);
    (*vec)->elem_size = elem_size;

    return ST_OK;
}

static ret_t vector_release(vector_t *vec) {
    da_release(vec->alloc);
    free(vec);

    return ST_OK;
}

static size_t vector_size(vector_t *vec) { return vec->size; }

static ret_t vector_add(vector_t *vec, const void *elem) {
    da_append(vec->alloc, (const char *) elem, vec->elem_size);
    vec->size++;

    return ST_OK;
}


static ret_t vector_get(vector_t *vec, size_t idx, void **elem) {
    if (idx >= vec->size)
        return ST_OUT_OF_RANGE;

    *elem = (void *) &(vec->alloc->ptr[vec->elem_size * idx]);

    return ST_OK;
}

static ret_t vector_set(vector_t *vec, size_t idx, void *elem) {
    if (idx >= vec->size)
        return ST_OUT_OF_RANGE;

    void *el = (void *) &(vec->alloc->ptr[vec->elem_size * idx]);

    memcpy(el, elem, vec->elem_size);

    return ST_OK;

}

typedef void(*vector_foreach_cb)(size_t, size_t, void *, void *);

static void vector_foreach(vector_t *vec, void *ctx, vector_foreach_cb cb) {
    size_t n = vec->size;
    for (size_t i = 0; i < n; ++i) {
        void *v = (void *) &(vec->alloc->ptr[vec->elem_size * i]);
        cb(i, vec->elem_size, ctx, v);
    }
}


//=============================================================================================
// HASH BINARY TREE
//=============================================================================================

typedef struct bt_node {
    uint64_t hash_key;
    void *data;
    struct bt_node *prev;
    struct bt_node *left;
    struct bt_node *right;
    data_release_cb rel_cb;

} bt_node_t;

static ret_t bt_node_create(bt_node_t **bt, uint64_t hash, void *data) {
    *bt = zalloc(sizeof(bt_node_t));
    bt_node_t *b = *bt;
    b->hash_key = hash;

    b->data = data;

    return ST_OK;
}

static ret_t bt_node_release(bt_node_t *bt, bool release_data) {

    if (bt != NULL) {

        bt_node_release(bt->left, release_data);
        bt_node_release(bt->right, release_data);


        if (release_data)
            bt->rel_cb(bt->data);

        safe_free(bt);

    }

    return ST_OK;
}


static ret_t bt_node_set(bt_node_t **bt, bt_node_t *prev, uint64_t hash, void *data, data_release_cb rel_cb) {
    if (*bt == NULL) {
        *bt = zalloc(sizeof(bt_node_t));
        bt_node_t *b = *bt;
        b->hash_key = hash;
        b->prev = prev;
        b->rel_cb = rel_cb;

        b->data = data;

        return ST_OK;
    } else {
        bt_node_t *b = *bt;

        if (b->hash_key > hash)
            return bt_node_set(&b->right, b, hash, data, rel_cb);
        else if (b->hash_key < hash)
            return bt_node_set(&b->left, b, hash, data, rel_cb);
        else {

            if ((*bt)->rel_cb) (*bt)->rel_cb(b->data);

            b->data = data;

            return ST_OK;
        }

    }
}


static ret_t bt_node_get(bt_node_t *bt, uint64_t hash, void **data) {
    if (bt == NULL)
        return ST_NOT_FOUND;

    if (bt->hash_key > hash)
        return bt_node_get(bt->right, hash, data);
    else if (bt->hash_key < hash)
        return bt_node_get(bt->left, hash, data);
    else {
        *data = bt->data;
        return ST_OK;
    }
}

static ret_t bt_node_left(bt_node_t *bt, bt_node_t **left) {
    bt_node_t *cur = bt;

    while (cur && cur->left)
        cur = cur->left;

    *left = cur;

    return ST_OK;
}

typedef void(*bt_node_traverse_cb)(bt_node_t *);

static void bt_node_traverse(bt_node_t *bt, bt_node_traverse_cb cb) {
    if (bt == NULL) return;

    bt_node_traverse(bt->left, cb);
    bt_node_traverse(bt->right, cb);

    cb(bt);
}

// froentends

typedef struct binary_tree {
    bt_node_t *head;
    data_release_cb rel_cb;
} binary_tree_t;


static ret_t bt_init(binary_tree_t **bt, data_release_cb cb) {
    *bt = zalloc(sizeof(binary_tree_t));
    (*bt)->rel_cb = cb;

    return ST_OK;
}

static ret_t bt_release(binary_tree_t *bt, bool release_data) {

    if (bt != NULL) {
        bt_node_release(bt->head, release_data);

        safe_free(bt);
    }

    return ST_OK;
}


//simple char*:int froentend

typedef struct str_int {
    const char *str;
    uint64_t i;
} str_int_t;

static str_int_t heap_str_int_decode(void *p) {
    str_int_t i;
    memcpy(&i, p, sizeof(str_int_t));

    return i;
}

static ret_t bt_si_set(binary_tree_t *bt, const char *key, uint64_t i) {
    uint64_t hash = crc64s(key);
    str_int_t *data = zalloc(sizeof(str_int_t));
    data->str = key;
    data->i = i;
    return bt_node_set(&bt->head, NULL, hash, data, bt->rel_cb);
}

static void bt_si_release(void *p) {
    str_int_t *data = (str_int_t *) p;
    free(data);
}

static ret_t bt_si_get(binary_tree_t *bt, const char *key, str_int_t **i) {
    uint64_t hash = crc64s(key);
    void *p = NULL;
    ret_t ret = bt_node_get(bt->head, hash, &p);
    if (ret != ST_OK)
        return ret;

    *i = (str_int_t *) p;

    return ret;
}

static void bt_si_traverse_cb(bt_node_t *node) {
    static ret_t counter = 1;
    str_int_t i = heap_str_int_decode(node->data);

    LOG_DEBUG("[%d][%08lX] %s : 0x%lX", counter++, node->hash_key, i.str, i.i);
}


static void bt_si_traverse(binary_tree_t *bt) {
    bt_node_traverse(bt->head, &bt_si_traverse_cb);
}

//=============================================================================================
// TESTS
//=============================================================================================


static ret_t test_split();

static void test_hash_bt();

static void test_da();

static void test_string();

static void test_ret();


static void tests_run() {
    test_ret();
    test_da();
    test_string();
    test_hash_bt();
    test_fifo();
    test_lifo();
    test_list();
    test_split();

}

static void test_hash_bt() {
    binary_tree_t *bt;
    str_int_t *node = NULL;
    bt_init(&bt, &bt_si_release);

    bt_si_set(bt, "1", 1);
    bt_si_set(bt, "2", 2);
    bt_si_set(bt, "3", 3);
    bt_si_set(bt, "4", 4);
    bt_si_set(bt, "5", 5);
    bt_si_set(bt, "6", 6);
    bt_si_set(bt, "7", 7);
    bt_si_set(bt, "8", 8);
    bt_si_set(bt, "9", 9);
    bt_si_set(bt, "10", 0xA);
    bt_si_set(bt, "11", 0xB);
    bt_si_set(bt, "12", 0xC);
    bt_si_set(bt, "13", 0xD);
    bt_si_set(bt, "14", 0xE);
    bt_si_set(bt, "15", 0xF);


    bt_si_get(bt, "1", &node);
    ASSERT(node->i == 0x1);
    bt_si_get(bt, "2", &node);
    ASSERT(node->i == 0x2);
    bt_si_get(bt, "3", &node);
    ASSERT(node->i == 0x3);
    bt_si_get(bt, "4", &node);
    ASSERT(node->i == 0x4);
    bt_si_get(bt, "5", &node);
    ASSERT(node->i == 0x5);
    bt_si_get(bt, "6", &node);
    ASSERT(node->i == 0x6);
    bt_si_get(bt, "7", &node);
    ASSERT(node->i == 0x7);
    bt_si_get(bt, "8", &node);
    ASSERT(node->i == 0x8);
    bt_si_get(bt, "9", &node);
    ASSERT(node->i == 0x9);
    bt_si_get(bt, "10", &node);
    ASSERT(node->i == 0xA);
    bt_si_get(bt, "11", &node);
    ASSERT(node->i == 0xB);
    bt_si_get(bt, "12", &node);
    ASSERT(node->i == 0xC);
    bt_si_get(bt, "13", &node);
    ASSERT(node->i == 0xD);
    bt_si_get(bt, "14", &node);
    ASSERT(node->i == 0xE);
    bt_si_get(bt, "15", &node);
    ASSERT(node->i == 0xF);

    bt_si_get(bt, "11", &node);
    node->i = 0xB;
    bt_si_get(bt, "12", &node);
    node->i = 0xC;
    bt_si_get(bt, "13", &node);
    node->i = 0xD;
    bt_si_get(bt, "14", &node);
    node->i = 0xE;
    bt_si_get(bt, "15", &node);
    node->i = 0xF;

    bt_si_get(bt, "11", &node);
    ASSERT(node->i == 0xB);
    bt_si_get(bt, "12", &node);
    ASSERT(node->i == 0xC);
    bt_si_get(bt, "13", &node);
    ASSERT(node->i == 0xD);
    bt_si_get(bt, "14", &node);
    ASSERT(node->i == 0xE);
    bt_si_get(bt, "15", &node);
    ASSERT(node->i == 0xF);


    bt_si_traverse(bt);
    bt_release(bt, true);
    bt_init(&bt, &bt_si_release);

    bt_si_set(bt, "1", 0);
    bt_si_set(bt, "2", 0);
    bt_si_set(bt, "3", 0);
    bt_si_set(bt, "4", 0);
    bt_si_set(bt, "5", 0);
    bt_si_set(bt, "6", 0);
    bt_si_set(bt, "7", 0);
    bt_si_set(bt, "8", 0);
    bt_si_set(bt, "9", 0);
    bt_si_set(bt, "10", 0x0);
    bt_si_set(bt, "11", 0x0);
    bt_si_set(bt, "12", 0x0);
    bt_si_set(bt, "13", 0x0);
    bt_si_set(bt, "14", 0x0);
    bt_si_set(bt, "15", 0x0);

    bt_si_get(bt, "1", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "2", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "3", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "4", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "5", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "6", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "7", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "8", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "9", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "10", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "11", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "12", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "13", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "14", &node);
    ASSERT(node->i == 0);
    bt_si_get(bt, "15", &node);
    ASSERT(node->i == 0);


    bt_si_get(bt, "10", &node);
    node->i = 0xA;
    bt_si_get(bt, "11", &node);
    node->i = 0xB;
    bt_si_get(bt, "12", &node);
    node->i = 0xC;
    bt_si_get(bt, "13", &node);
    node->i = 0xD;
    bt_si_get(bt, "14", &node);
    node->i = 0xE;
    bt_si_get(bt, "15", &node);
    node->i = 0xF;

    bt_si_get(bt, "10", &node);
    ASSERT(node->i == 0xA);
    bt_si_get(bt, "11", &node);
    ASSERT(node->i == 0xB);
    bt_si_get(bt, "12", &node);
    ASSERT(node->i == 0xC);
    bt_si_get(bt, "13", &node);
    ASSERT(node->i == 0xD);
    bt_si_get(bt, "14", &node);
    ASSERT(node->i == 0xE);
    bt_si_get(bt, "15", &node);
    ASSERT(node->i == 0xF);

    bt_si_traverse(bt);

    bt_release(bt, true);
    bt = NULL;
}

static ret_t test_split() {

    string *text = NULL;
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


    list_t *blklist = NULL;
    string_split(text, '\n', &blklist);

    LOG_TRACE("------- LINES OF TOKENS -----------");
    slist_fprint(blklist);

    string *tk = NULL;
    list_iter_t *blkit = NULL;
    list_iter_init(blklist, &blkit);

    while ((tk = slist_next(blkit)) != NULL) {

        LOG_TRACE("------- TOKEN LINE-----------");
        string_printt(tk);

        LOG_TRACE("------- TOKEN SPLIT-----------");

        list_t *tokens = NULL;
        string_split(tk, ' ', &tokens);

        slist_fprint(tokens);

        string *s = NULL;
        list_iter_t *token_it = NULL;
        list_iter_init(blklist, &token_it);

        while ((s = slist_next(token_it)) != NULL) {
            list_t *kv = NULL;
            string_split(s, '=', &kv);

            LOG_TRACE("------- TOKEN KEY-VALUE -----------");
            slist_fprint(kv);


            string *key = (string *) kv->head->data;
            string *val = (string *) kv->head->next->data;

            string_strip(val);

            LOG_TRACE("------- STRIPED TOKEN KEY-VALUE -----------");
            string_printt(key);
            string_printt(string_null);

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

//========================================================================


enum {
    /// These values increment when an I/O request completes.
            READ_IO = 0, ///requests - number of read I/Os processed

    /// These values increment when an I/O request is merged with an already-queued I/O request.
            READ_MERGE = 1, /// requests - number of read I/Os merged with in-queue I/O

    /// These values count the number of sectors read from or written to this block device.
    /// The "sectors" in question are the standard UNIX 512-byte sectors, not any device- or
    /// filesystem-specific block size.
    /// The counters are incremented when the I/O completes.
            READ_SECTORS = 2, /// requests - number of read I/Os merged with in-queue I/O


    /// These values count the number of milliseconds that I/O requests have
    /// waited on this block device.  If there are multiple I/O requests waiting,
    /// these values will increase at a rate greater than 1000/second; for
    /// example, if 60 read requests wait for an average of 30 ms, the read_ticks
    /// field will increase by 60*30 = 1800.
            READ_TICKS = 3, ///milliseconds - total wait time for read requests

    /// These values increment when an I/O request completes.
            WRITE_IO = 4, /// requests - number of write I/Os processed

    /// These values increment when an I/O request is merged with an already-queued I/O request.
            WRITE_MERGES = 5, /// requests - number of write I/Os merged with in-queue I/O

    /// These values count the number of sectors read from or written to this block device.
    /// The "sectors" in question are the standard UNIX 512-byte sectors, not any device- or
    /// filesystem-specific block size.
    /// The counters are incremented when the I/O completes.
            WRITE_SECTORS = 6, /// sectors - number of sectors written


    /// These values count the number of milliseconds that I/O requests have
    /// waited on this block device.  If there are multiple I/O requests waiting,
    /// these values will increase at a rate greater than 1000/second; for
    /// example, if 60 read requests wait for an average of 30 ms, the read_ticks
    /// field will increase by 60*30 = 1800.
            WRITE_TICKS = 7, /// milliseconds - total wait time for write requests

    /// This value counts the number of I/O requests that have been issued to
    /// the device driver but have not yet completed.  It does not include I/O
    /// requests that are in the queue but not yet issued to the device driver.
            IN_FLIGHT = 8, /// requests - number of I/Os currently in flight

    /// This value counts the number of milliseconds during which the device has
    /// had I/O requests queued.
            IO_TICKS = 9, /// milliseconds - total time this block device has been active

    /// This value counts the number of milliseconds that I/O requests have waited
    /// on this block device.  If there are multiple I/O requests waiting, this
    /// value will increase as the product of the number of milliseconds times the
    /// number of requests waiting (see "read ticks" above for an example).
            TIME_IN_QUEUE = 10 /// milliseconds - total wait time for all requests
};


static size_t get_sfile_size(const char *filename) {
    struct stat st;
    stat(filename, &st);
    return (size_t) st.st_size;
}

typedef void(*cmd_exec_cb)(void *, list_t *);

static void file_mmap_string(const char *filename, string *s) {
    size_t filesize = get_sfile_size(filename);
    //Open file
    int fd = open(filename, O_RDONLY, 0);

    //Execute mmap
    void *data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);

    //Write the mmapped data
    string_init(&s);
    string_appendn(s, (char *) data, filesize);

    //Cleanup
    munmap(data, filesize);
    close(fd);
}


static size_t get_fd_file_size(int fd) {
    struct stat st;
    fstat(fd, &st);
    return (size_t) st.st_size;
}

static void fd_file_mmap(int fd, string *s) {
    size_t filesize = get_fd_file_size(fd);

    //Execute mmap
    void *data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);


    //Write the mmapped data
    string_init(&s);
    string_appendn(s, (char *) data, filesize);

    //Cleanup
    munmap(data, filesize);
    close(fd);
}


static void file_read_all(const char *filename, char **buff, size_t *size) {
    FILE *f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = malloc(fsize);
    fread(string, fsize, 1, f);
    fclose(f);

    *buff = string;
    *size = fsize;
}

static void file_read_all_s(const char *filename, string *s) {
    FILE *f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);

    da_realloc(_da(s), fsize);
    fread(_da(s)->ptr, fsize, 1, f);
    _da(s)->used = fsize;
    fclose(f);
}

enum {
    HR_SIZE_KB,
    HR_SIZE_MB,
    HR_SIZE_GB
};


void human_readable_size(uint64_t bytes, double *result, int *type) {

    double r = bytes / 1024.;
    if (r < 1024)  // KB / sec
    {
        *result = r;
        *type = HR_SIZE_KB;
        return;
    }

    r = bytes / 1024 / 1024;
    if (r < 1024)  // MiB / sec
    {
        *result = r;
        *type = HR_SIZE_MB;
        return;
    }

    r = bytes / 1024 / 1024 / 1024;
    {
        *result = r;
        *type = HR_SIZE_GB;
        return;
    }
}

//===============================================================================
// DEVICE MANAGMENT
//===============================================================================

typedef struct device {
    string *name;
    //struct statvfs stats;
    uint64_t stat[11];
    string *perf_read;
    string *perf_write;
    string *label;
    uint64_t size;
    uint64_t used;
    uint64_t avail;
    uint64_t use;
    double perc;
    string *fs;
    string *mount;
    string *sysfolder;
    string *model;
    string *uuid;
    string *shed;
} device_t;

static void device_release_cb(void *p) {
    device_t *dev = (device_t *) p;

    if (dev->name) string_release(dev->name);
    if (dev->perf_read) string_release(dev->perf_read);
    if (dev->perf_write) string_release(dev->perf_write);
    if (dev->label) string_release(dev->label);
    if (dev->fs) string_release(dev->fs);
    if (dev->mount) string_release(dev->mount);
    if (dev->sysfolder) string_release(dev->sysfolder);
    if (dev->model) string_release(dev->model);
    if (dev->uuid) string_release(dev->uuid);
    if (dev->shed) string_release(dev->shed);

    free(dev);
}

static device_t *device_list_search(list_t *devs, string *name) {
    list_iter_t *it = NULL;
    list_iter_init(devs, &it);

    device_t *dev;
    while ((dev = list_iter_next(it))) {
        string *dev_devname = NULL;
        string_create(&dev_devname, "/dev/");
        string_add(dev_devname, dev->name);

        ret_f_t res = return_map(string_compare(dev_devname, name));

        string_release(dev_devname);


        if (res.code == ST_OK && res.data == 0) {
            break;
        }
    }


    list_iter_release(it);

    return dev;
}

static device_t *device_list_direct_search(list_t *devs, string *name) {
    list_iter_t *it = NULL;
    list_iter_init(devs, &it);

    device_t *dev;
    while ((dev = list_iter_next(it))) {


        ret_f_t res = return_map(string_compare(dev->name, name));
        if (res.code == ST_OK && res.data == 0) {
            break;
        }
    }


    list_iter_release(it);

    return dev;
}

static void device_diff(device_t *a, device_t *b, double sample_size) {
    static const uint64_t BLOCK_SIZE = 512; // Unix block size

    b->stat[WRITE_SECTORS] = b->stat[WRITE_SECTORS] - a->stat[WRITE_SECTORS];
    b->stat[READ_SECTORS] = b->stat[READ_SECTORS] - a->stat[READ_SECTORS];

    double hr_size = 0.0;
    int size_type = -1;
    human_readable_size(b->stat[READ_SECTORS] * BLOCK_SIZE, &hr_size, &size_type);

    hr_size /= sample_size;

    string *speed = NULL;
    string_init(&speed);
    switch (size_type) {
        case HR_SIZE_KB:
            string_appendf(speed, "%06.2f Kb/s", hr_size);
            break;
        case HR_SIZE_MB:
            string_appendf(speed, "%06.2f Mb/s", hr_size);
            break;
        case HR_SIZE_GB:
            string_appendf(speed, "%06.2f Gb/s", hr_size);
            break;
        default:
            break;
    }

    if (b->perf_read) string_release(b->perf_read);
    b->perf_read = speed;


    human_readable_size(b->stat[WRITE_SECTORS] * BLOCK_SIZE, &hr_size, &size_type);

    hr_size /= sample_size;

    string_init(&speed);
    switch (size_type) {
        case HR_SIZE_KB:
            string_appendf(speed, "%06.2f Kb/s", hr_size);
            break;
        case HR_SIZE_MB:
            string_appendf(speed, "%06.2f Mb/s", hr_size);
            break;
        case HR_SIZE_GB:
            string_appendf(speed, "%06.2f Gb/s", hr_size);
            break;
        default:
            break;
    }

    if (b->perf_write) string_release(b->perf_write);
    b->perf_write = speed;
}

static ret_t cmd_execute(const char *cmd, void *ctx, cmd_exec_cb cb) {
    FILE *fpipe;

    if (!(fpipe = popen(cmd, "r")))
        return ST_ERR;

    char line[1024] = {0};


    list_t *sl;
    list_init(&sl, &string_release_cb);

    while (fgets(line, sizeof(line), fpipe)) {
        string *s = NULL;

        string_create(&s, line);
        list_push(sl, s);
    }

    cb(ctx, sl);

    pclose(fpipe);

    return ST_OK;
}


enum {
    DFS_NAME = 0,
    DFS_TOTAL = 1,
    DFS_USED = 2,
    DFS_AVAIL = 3,
    DFS_USE = 4

};


typedef struct df_stat {
    string *dev;

    uint64_t total;
    uint64_t used;
    uint64_t avail;
    uint64_t use;


} df_stat_t;

typedef struct {
    list_t *devs;
    uint64_t skip_first;
} df_t;


static void df_init(list_t *devs, df_t **df) {
    *df = zalloc(sizeof(df_t));
    (*df)->devs = devs;
    (*df)->skip_first = 1;
}

static ret_t ht_set_df(hashtable_t *ht, string *key, df_stat_t *stat) {

    ht_key_t *hkey = NULL;
    uint64_t hash = crc64(0, (const uint8_t *) string_cdata(key), string_size(key));
    ht_create_key_i(hash, &hkey);

    ht_value_t *val = NULL;
    ht_create_value(stat, sizeof(df_stat_t), &val);

    ht_set(ht, hkey, val);

    return ST_OK;
}

static void df_callback(void *ctx, list_t *lines) {
    df_t *df = (df_t *) ctx;

    LOG_TRACE("------- LINES OF TOKENS -----------");
    slist_fprint(lines);

    string *tk = NULL;
    list_iter_t *line_it = NULL;
    list_iter_init(lines, &line_it);
    while ((tk = slist_next(line_it))) {

        LOG_TRACE("------- TOKEN LINE-----------");
        string_printt(tk);


        if (!string_re_match(tk, ".*(sd.*).*"))
            continue;

        LOG_TRACE("------- TOKEN SPLIT-----------");

        list_t *tokens = NULL;
        string_split(tk, ' ', &tokens);

        slist_fprint(tokens);


        list_iter_t *tk_it = NULL;
        list_iter_init(tokens, &tk_it);


        string *s = NULL;
        uint64_t k = 0;
        device_t *dev = NULL;
        while ((s = slist_next(tk_it)) != NULL) {

            string_strip(s);

            if (k == DFS_NAME) {
                dev = device_list_search(df->devs, s);
                if (dev == NULL)
                    break;
            }

            switch (k) {
                case DFS_TOTAL:
                    string_to_u64(s, &dev->size);
                    break;
                case DFS_USED: {
                    string_to_u64(s, &dev->used);
                    if (dev->size)
                        dev->perc = dev->used / (double) dev->size * 100.0;
                    break;
                }
                case DFS_AVAIL:
                    string_to_u64(s, &dev->avail);
                    break;
                case DFS_USE: {
                    string_pop_head(s, 1);
                    string_to_u64(s, &dev->use);
                    break;
                }

                default:
                    break;
            }

            ++k;

        }

        list_iter_release(tk_it);
        list_release(tokens, true);

    }

    list_iter_release(line_it);
    list_release(lines, true);


}

static void df_execute(df_t *dfs) {

    const char *cmd = "df --block-size=1";
    cmd_execute(cmd, dfs, &df_callback);
}


enum {
    BLK_NAME = 0,
    BLK_FSTYPE,
    BLK_SCHED,
    BLK_SIZE,
    BLK_MODEL,
    BLK_LABEL,
    BLK_UUID,
    BLK_MOUNTPOINT,
    BLK_LAST
};

typedef struct sblkid {
    list_t *devs;
} sblkid_t;

typedef struct {
    char *ns;
    char *ne;
    char *vs;
    char *ve;
} kv_pair;

enum {
    SM_CHAR = 0,
    SM_EQUAL,
    SM_QUOTE,
    SM_SPACE
};


static ret_t sm_token(char ch) {
    if (ch == '"') return SM_QUOTE;
    if (ch == '=') return SM_EQUAL;
    if (isspace(ch)) return SM_SPACE;
    return SM_CHAR;
}

#define FSM_CALLBACK(x) ((size_t)&x)


typedef struct system {
    hashtable_t *blk;
} system_t;


static ret_t system_init(system_t **sys) {
    *sys = zalloc(sizeof(system_t));
    ht_init(&(*sys)->blk);

    return ST_OK;
}

static ret_t vector_add_kv(vector_t *vec, string *key, string *val) {
    skey_value_t *kv = zalloc(sizeof(skey_value_t));
    string_dub(key, &kv->key);
    string_dub(val, &kv->value);

    vector_add(vec, kv);

    return ST_OK;
}

static ret_t ht_set_s(hashtable_t *ht, string *key, vector_t *vec) {

    ht_key_t *hkey = NULL;
    uint64_t hash = crc64(0, (const uint8_t *) string_cdata(key), string_size(key));
    ht_create_key_i(hash, &hkey);

    ht_value_t *val = NULL;
    ht_create_value(vec, sizeof(vector_t), &val);

    ht_set(ht, hkey, val);

    return ST_OK;
}

typedef struct string_string_pair {
    string *key;
    string *val;
} ss_kv_t;


static void ss_kv_release_cb(void *p) {
    ss_kv_t *kv = (ss_kv_t *) p;
    string_release(kv->key);
    string_release(kv->val);
    free(kv);
}

static void sblk_callback(void *ctx, list_t *lines) {

    sblkid_t *sys = (sblkid_t *) ctx;


    string *tk = NULL;
    list_iter_t *ln_it = NULL;
    list_iter_init(lines, &ln_it);

    while ((tk = slist_next(ln_it))) {

        LOG_INFO("------- TOKEN LINE-----------");
        string_printt(tk);


        //================================
        regex_t re;
        regex_compile(&re, "(\\w+)=\"([[:alnum:][:space:]/-]*)\"");

        char *tkp = NULL;
        string_create_nt(tk, &tkp);

        device_t *dev = NULL;
        list_t *pairs = NULL;
        list_init(&pairs, &ss_kv_release_cb);

        const char *p = tkp;
        /* "N_matches" is the maximum number of matches allowed. */
        const int n_matches = 5;
        /* "M" contains the matches found. */
        regmatch_t m[n_matches];
        while (1) {
            int nomatch = regexec(&re, p, n_matches, m, 0);
            if (nomatch) {
                LOG_INFO("No more matches.");
                break;
            }

            string *key = NULL;
            string *val = NULL;
            ss_kv_t *ss_kv = zalloc(sizeof(ss_kv_t));
            for (int i = 0; i < n_matches; i++) {
                int start;
                int finish;
                if (m[i].rm_so == -1) {
                    break;
                }

                start = (int) (m[i].rm_so + (p - tkp));
                finish = (int) (m[i].rm_eo + (p - tkp));
                if (i == 0) {
                    continue;
                }
                if (i == 1) {
                    string_init(&key);
                    string_appendn(key, tkp + start, (finish - start));
                    ss_kv->key = key;

                } else if (i == 2) {

                    if (finish - start != 0) {

                        string_init(&val);
                        string_appendn(val, tkp + start, (finish - start));
                        ss_kv->val = val;

                        list_push(pairs, ss_kv);
                    } else {
                        string_release(key);
                        free(ss_kv);
                    }
                }


            }
            p += m[0].rm_eo;
        }

        regfree(&re);
        free(tkp);


        //================================
        list_iter_t *kv_it = NULL;
        list_iter_init(pairs, &kv_it);

        ss_kv_t *kv;
        while ((kv = list_iter_next(kv_it))) {


            string_printd(kv->key);
            string_printd(kv->val);

            if (string_comparez(kv->key, "NAME") == RET_OK) {
                dev = device_list_direct_search(sys->devs, kv->val);
            } else if (string_comparez(kv->key, "SCHED") == RET_OK) {
                if (dev)
                    string_dub(kv->val, &dev->shed);

            } else if (string_comparez(kv->key, "FSTYPE") == RET_OK) {
                if (dev)
                    string_dub(kv->val, &dev->fs);

            } else if (string_comparez(kv->key, "MODEL") == RET_OK) {
                if (dev) {
                    string_strip(kv->val);
                    string_dub(kv->val, &dev->model);
                }
            } else if (string_comparez(kv->key, "MOUNTPOINT") == RET_OK) {
                if (dev)
                    string_dub(kv->val, &dev->mount);
            } else if (string_comparez(kv->key, "UUID") == RET_OK) {
                if (dev)
                    string_dub(kv->val, &dev->uuid);
            } else if (string_comparez(kv->key, "LABEL") == RET_OK) {
                if (dev)
                    string_dub(kv->val, &dev->label);
            } else if (string_comparez(kv->key, "SIZE") == RET_OK) {
                if (dev && dev->size == 0) {
                    string_to_u64(kv->val, &dev->size);
                }
            }

        }

        list_iter_release(kv_it);
        list_release(pairs, true);

        //================================
    }

    list_iter_release(ln_it);
    list_release(lines, true);

    /**
    return;

    fprintf(stdout, "------- LINES OF TOKENS -----------\n");
    slist_fprint(lines);
    fflush(stdout);

    string *tk = NULL;
    list_iter_t* ln_it = NULL;
    list_iter_init(lines, &ln_it);
    while ((tk = slist_next(ln_it)) != NULL) {

        fprintf(stdout, "------- TOKEN LINE-----------\n");
        string_print(tk);

        fprintf(stdout, "------- TOKEN SPLIT-----------\n");

        list_t *tokens = NULL;
        string_split(tk, ' ', &tokens);


        slist_fprint(tokens);

        list_iter_t* tk_it = NULL;
        list_iter_init(tokens, &tk_it);

        //==============================
        // vector pair init

        vector_t *vec = NULL;
        vector_init(&vec, sizeof(skey_value_t));

        //==============================


        string *s = NULL;
        string *sname = NULL;
        while ((s = slist_next(tk_it)) != NULL) {


            list_t *kv = NULL;
            string_split(s, '=', &kv);

            fprintf(stdout, "------- TOKEN KEY-VALUE -----------\n");
            slist_fprint(kv);


            list_iter_t* kv_it = NULL;
            list_iter_init(kv, &kv_it);

            string *key = slist_next(kv_it);
            string *val = slist_next(kv_it);

            string_strip(val);

            if (string_comparez(key, "NAME") == ST_OK)
                string_dub(val, &sname);

            fprintf(stdout, "------- STRIPED TOKEN KEY-VALUE -----------\n");

            //add kv
            vector_add_kv(vec, key, val);

            string_print(key);
            string_print(val);

            list_release(kv, true); kv = NULL;
        }


        //add to ht, not need to free
        ht_set_s(sys->blk, sname, vec);

        string_release(sname); sname = NULL;

        list_release(tokens, true); tokens = NULL;

    }
    **/

}

static ret_t sblk_execute(sblkid_t *sblk) {
    static const char *options[] = {"NAME", "FSTYPE", "SCHED", "SIZE", "MODEL", "LABEL", "UUID", "MOUNTPOINT"};

    string *cmd = NULL;
    string_create(&cmd, "lsblk -i -P -b -o ");
    string_append(cmd, options[0]);


    size_t opt_size = sizeof(options) / sizeof(char *);
    for (size_t i = 1; i < opt_size; ++i) {
        string_append(cmd, ",");
        string_append(cmd, options[i]);
    }

    string_append(cmd, "");


    char *ccmd = string_makez(cmd);
    cmd_execute(ccmd, sblk, &sblk_callback);

    string_release(cmd);
    free(ccmd);
    return ST_OK;
}


static void scan_dir_dev(string *basedir, list_t *devs) {
    struct dirent *dir = NULL;

    char *dir_c = string_makez(basedir);
    DIR *d = opendir(dir_c);
    free(dir_c);

    if (d) {
        while ((dir = readdir(d)) != NULL) {

            string *dir_name = NULL;
            string_create(&dir_name, dir->d_name);

            bool match = string_re_match(dir_name, "sd.*");

            if (match) {

                device_t *dev = zalloc(sizeof(device_t));

                // create sysdir
                string *sysdir = NULL;
                string_init(&sysdir);
                string_add(sysdir, basedir);
                string_add(sysdir, dir_name);
                string_append(sysdir, "/");

                // set name and sysdir
                string_dub(dir_name, &dev->name);
                dev->sysfolder = sysdir;

                // getting stat
                string *stat_s = NULL;
                string_init(&stat_s);

                string *stat_filename = NULL;
                string_dub(sysdir, &stat_filename);
                string_append(stat_filename, "stat");

                char *stat_filename_c = string_makez(stat_filename);
                file_read_all_s(stat_filename_c, stat_s);
                string_strip(stat_s);

                free(stat_filename_c);
                string_release(stat_filename);

                list_t *lstat_s = NULL;
                string_split(stat_s, ' ', &lstat_s);

                string_release(stat_s);

                list_iter_t *lstat_it = NULL;
                list_iter_init(lstat_s, &lstat_it);


                string *s;
                size_t stat_n = 0;
                while ((s = (string *) list_iter_next(lstat_it)))
                    string_to_u64(s, &dev->stat[stat_n++]);


                list_iter_release(lstat_it);
                list_release(lstat_s, true);


                // add dev to list
                list_push(devs, dev);

                string *subdir = NULL;
                string_dub(sysdir, &subdir);

                // recursive iterate
                scan_dir_dev(subdir, devs);

                string_release(subdir);
            }

            string_release(dir_name);
        }

        closedir(d);
    }
}


#define COLON_OFFSET 1
#define COLON_DEVICE COLON_OFFSET
#define COLON_READ  (9 + COLON_OFFSET)
#define COLON_WRITE (22 + COLON_OFFSET)
#define COLON_SIZE (35 + COLON_OFFSET)
#define COLON_USE  (46 + COLON_OFFSET)
#define COLON_PERC (57 + COLON_OFFSET)
#define COLON_FILESYSTEM (64 + COLON_OFFSET)
#define COLON_SCHED (70 + COLON_OFFSET)
#define COLON_MOUNT (75 + COLON_OFFSET)
#define COLON_MODEL (83 + COLON_OFFSET)


static list_t *ldevices = NULL;
static atomic_bool programm_exit = false;
static atomic_ulong sample_rate_mul = 10;
static pthread_mutex_t ldevices_mtx;


static inline double device_get_sample_rate() {
    return DEVICE_BASE_SAMPLE_RATE * atomic_load(&sample_rate_mul);
}

static void ncruses_print_hr(int row, int col, uint64_t value) {
    double size = 0.0;
    int type = 0;
    human_readable_size(value, &size, &type);

    char buffer[128] = {0};
    switch (type) {
        case HR_SIZE_KB:
            sprintf(buffer, "%06.2f Kb", size);
            break;
        case HR_SIZE_MB:
            sprintf(buffer, "%06.2f Mb", size);
            break;
        case HR_SIZE_GB:
            sprintf(buffer, "%06.2f Gb", size);
            break;
        default:
            break;
    }

    mvaddstr(row, col, buffer);
}

void *ncurses_keypad(void *p) {
    int highlight = 1;
    int choice = 0;
    int c;
    while (1) {
        c = wgetch(stdscr);
        switch (c) {
            case KEY_F(10):
                atomic_store(&programm_exit, true);
                return p;
                break;
            case KEY_UP:
                atomic_fetch_add(&sample_rate_mul, 1);
                break;
            case KEY_DOWN: {
                while (1) {
                    ulong mul = atomic_load(&sample_rate_mul);
                    if (mul > 1) {
                        if (atomic_compare_exchange_strong(&sample_rate_mul, &mul, mul - 1))
                            break;
                    } else {
                        break;
                    }
                }

                break;
            }

            default:
                break;
        }
    }

    return p;
}


void ncurses_window() {
    initscr();            /* Start curses mode 		  */

    if (has_colors() == FALSE) {
        endwin();
        LOG_ERROR("Your terminal does not support color");
        exit(EXIT_FAILURE);
    }

    raw();
    keypad(stdscr, TRUE);
    noecho();
    start_color();

    pthread_t keypad__thrd;
    pthread_create(&keypad__thrd, NULL, &ncurses_keypad, NULL);
    pthread_detach(keypad__thrd);

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);

    while (!atomic_load(&programm_exit)) {
        int row = 1;

        clear();

        attron(A_BOLD);
        attron(COLOR_PAIR(1));

        char hwversion_s[128] = {0};
        sprintf(hwversion_s, "HWMonitor %d.%02d", HW_VERSION_MAJOR, HW_VERSION_MINOR);
        mvaddstr(row++, 1, hwversion_s);

        mvaddstr(row++, 1, "Keypad: [UP - Increase sample rate][DOWN - Decrease sample rate][F10 Exit]");

        char samplesize_s[128] = {0};
        sprintf(samplesize_s, "Sample rate %05.3f sec", device_get_sample_rate());
        mvaddstr(row++, 1, samplesize_s);


        mvaddstr(++row, COLON_DEVICE, "Device");
        mvaddstr(row, COLON_READ, "Read");
        mvaddstr(row, COLON_WRITE, "Write");
        mvaddstr(row, COLON_SIZE, "Size");
        mvaddstr(row, COLON_USE, "Use");
        mvaddstr(row, COLON_PERC, "%");
        mvaddstr(row, COLON_FILESYSTEM, "FS");
        mvaddstr(row, COLON_SCHED, "Sch");
        mvaddstr(row, COLON_MOUNT, "Mount");
        mvaddstr(row++, COLON_MODEL, "Model");
        attroff(COLOR_PAIR(1));


        pthread_mutex_lock(&ldevices_mtx);

        if (ldevices == NULL) {
            pthread_mutex_unlock(&ldevices_mtx);


            usleep(100000);

            pthread_mutex_lock(&ldevices_mtx);
            if (ldevices == NULL) {

                pthread_mutex_unlock(&ldevices_mtx);
                continue;
            }

        }


        list_iter_t *it = NULL;
        list_iter_init(ldevices, &it);
        device_t *dev = NULL;
        while ((dev = list_iter_next(it))) {
            attron(COLOR_PAIR(2));

            char *name = string_makez(dev->name);
            char *syspath = string_makez(dev->sysfolder);
            char *fs = string_makez(dev->fs);
            char *model = string_makez(dev->model);
            char *mount = string_makez(dev->mount);
            char *uuid = string_makez(dev->uuid);
            char *label = string_makez(dev->label);
            char *shed = string_makez(dev->shed);
            char *read = string_makez(dev->perf_read);
            char *write = string_makez(dev->perf_write);

            char perc[32] = {0};
            sprintf(perc, "%04.1f%%", dev->perc);

            mvaddstr(row, COLON_DEVICE, name);
            mvaddstr(row, COLON_READ, read);
            mvaddstr(row, COLON_WRITE, write);
            ncruses_print_hr(row, COLON_SIZE, dev->size);
            ncruses_print_hr(row, COLON_USE, dev->used);
            mvaddstr(row, COLON_PERC, perc);
            mvaddstr(row, COLON_FILESYSTEM, fs);
            mvaddstr(row, COLON_SCHED, shed);
            mvaddstr(row, COLON_MOUNT, mount);
            mvaddstr(row++, COLON_MODEL, model);

            free(write);
            free(read);
            free(shed);
            free(label);
            free(uuid);
            free(mount);
            free(model);
            free(fs);
            free(syspath);
            free(name);

            attroff(COLOR_PAIR(2));
        }

        list_iter_release(it);

        pthread_mutex_unlock(&ldevices_mtx);

        attroff(A_BOLD);

        refresh(); // Print to the screen

        usleep(100000);
    }


    endwin();
}

static void check_style_defines() {
#ifdef _POSIX_SOURCE
    LOG_TRACE("_POSIX_SOURCE defined");
#endif

#ifdef _POSIX_C_SOURCE
    LOG_TRACE("_POSIX_C_SOURCE defined: %ldL", (long) _POSIX_C_SOURCE);
#endif

#ifdef _ISOC99_SOURCE
    LOG_TRACE("_ISOC99_SOURCE defined");
#endif

#ifdef _ISOC11_SOURCE
    LOG_TRACE("_ISOC11_SOURCE defined\n");
#endif

#ifdef _XOPEN_SOURCE
    LOG_TRACE("_XOPEN_SOURCE defined: %d\n", _XOPEN_SOURCE);
#endif

#ifdef _XOPEN_SOURCE_EXTENDED
    LOG_TRACE("_XOPEN_SOURCE_EXTENDED defined\n");
#endif

#ifdef _LARGEFILE64_SOURCE
    LOG_TRACE("_LARGEFILE64_SOURCE defined\n");
#endif

#ifdef _FILE_OFFSET_BITS
    LOG_TRACE("_FILE_OFFSET_BITS defined: %d\n", _FILE_OFFSET_BITS);
#endif

#ifdef _BSD_SOURCE
    LOG_TRACE("_BSD_SOURCE defined\n");
#endif

#ifdef _SVID_SOURCE
    LOG_TRACE("_SVID_SOURCE defined\n");
#endif

#ifdef _ATFILE_SOURCE
    LOG_TRACE("_ATFILE_SOURCE defined\n");
#endif

#ifdef _GNU_SOURCE
    LOG_TRACE("_GNU_SOURCE defined\n");
#endif

#ifdef _REENTRANT
    LOG_TRACE("_REENTRANT defined\n");
#endif

#ifdef _THREAD_SAFE
    LOG_TRACE("_THREAD_SAFE defined\n");
#endif

#ifdef _FORTIFY_SOURCE
    LOG_TRACE("_FORTIFY_SOURCE defined\n");
#endif
}

static void devices_get(list_t **devs) {
    string *basedir = NULL;

    list_init(devs, &device_release_cb);

    string_create(&basedir, "/sys/block/");

    scan_dir_dev(basedir, *devs);

    string_release(basedir);

    df_t *df;
    df_init(*devs, &df);
    df_execute(df);
    free(df);

    sblkid_t blk;
    blk.devs = *devs;
    sblk_execute(&blk);

#ifndef NDEBUG
    list_iter_t* list_it = NULL;
    list_iter_init(*devs, &list_it);

    device_t* dev;
    while((dev = (device_t*)list_iter_next(list_it)))
    {
        char* name = string_makez(dev->name);
        char* syspath = string_makez(dev->sysfolder);
        char* fs = string_makez(dev->fs);
        char* model = string_makez(dev->model);
        char* mount = string_makez(dev->mount);
        char* uuid = string_makez(dev->uuid);
        char* label = string_makez(dev->label);
        LOG_DEBUG("[0x%p][name=%s][syspath=%s][size=%lu][used=%lu][avail=%lu][use=%lu][perc=%lf][fs=%s][model=%s]"
                          "[mount=%s][uuid=%s][label=%s]\n",
                  (void*)dev, name, syspath,
                  dev->size, dev->used, dev->avail, dev->use, dev->perc,
                  fs, model, mount, uuid, label

        );

        free(label);
        free(uuid);
        free(mount);
        free(model);
        free(fs);
        free(syspath);
        free(name);
    }

    list_iter_release(list_it);
#endif
}


typedef void(*sampled_device_cb)(list_t *);


static void devices_sample(double sample_size_sec, sampled_device_cb cb) {
    list_t *devs_a = NULL;
    list_t *devs_b = NULL;

    devices_get(&devs_a);

    __useconds_t ustime = (__useconds_t) (sample_size_sec * 1000000.0);
    usleep(ustime);


    devices_get(&devs_b);


    list_iter_t *it = NULL;
    list_iter_init(devs_a, &it);

    device_t *dev_a;
    device_t *dev_b;
    while ((dev_a = list_iter_next(it))) {
        list_iter_t *it2 = NULL;
        list_iter_init(devs_b, &it2);

        while ((dev_b = list_iter_next(it2))) {
            ret_f_t cmp = return_map(string_compare(dev_a->name, dev_b->name));
            if (cmp.code == ST_OK && cmp.data == 0) {
                device_diff(dev_a, dev_b, sample_size_sec);
                break;
            }

        }

        list_iter_release(it2);
    }

    list_iter_release(it);

    list_release(devs_a, true);

    if (cb) cb(devs_b);

    pthread_mutex_lock(&ldevices_mtx);
    if (ldevices) list_release(ldevices, true);
    ldevices = devs_b;
    pthread_mutex_unlock(&ldevices_mtx);

}


void test_sampled_device(list_t *devs) {
    list_iter_t *list_it = NULL;
    list_iter_init(devs, &list_it);

    LOG_DEBUG("============SAMPLED DEVICES=============");

    device_t *dev;
    while ((dev = (device_t *) list_iter_next(list_it))) {
        char *name = string_makez(dev->name);
        char *syspath = string_makez(dev->sysfolder);
        char *fs = string_makez(dev->fs);
        char *model = string_makez(dev->model);
        char *mount = string_makez(dev->mount);
        char *uuid = string_makez(dev->uuid);
        char *label = string_makez(dev->label);
        char *read = string_makez(dev->perf_read);
        char *write = string_makez(dev->perf_write);
        LOG_DEBUG("[0x%p][name=%s][syspath=%s][size=%lu][used=%lu][avail=%lu][use=%lu][perc=%lf][fs=%s][model=%s]"
                          "[mount=%s][uuid=%s][label=%s][read=%s][write=%s]\n",
                  (void *) dev, name, syspath,
                  dev->size, dev->used, dev->avail, dev->use, dev->perc,
                  fs, model, mount, uuid, label,
                  read, write

        );

        free(write);
        free(read);
        free(label);
        free(uuid);
        free(mount);
        free(model);
        free(fs);
        free(syspath);
        free(name);
    }

    list_iter_release(list_it);
}

void *start_device_sample(void *p) {
    while (!atomic_load(&programm_exit)) {
        double sample_rate = device_get_sample_rate();
#ifndef NDEBUG
        devices_sample(sample_rate, &test_sampled_device);
#else
        devices_sample(sample_rate, NULL);
#endif
    }

    return p;
}

void sig_handler(int signo) {
    if (signo == SIGTERM || signo == SIGINT) {
        LOG_ERROR("Stopping the programm");

        // needed be an atomic
        atomic_store(&programm_exit, true);
    }

}

//=======================================================================================
// MAIN
//=======================================================================================

int main() {

#ifdef ENABLE_LOGGING
    log_init(LOGLEVEL_NONE, "HWMonitor.log");
#endif

    init_gloabls();

#ifndef NDEBUG
    check_style_defines();
    tests_run();
#endif

    pthread_mutex_init(&ldevices_mtx, NULL);

    pthread_t t;
    pthread_create(&t, NULL, &start_device_sample, NULL);

    signal(SIGINT, &sig_handler);
    signal(SIGTERM, &sig_handler);
    signal(SIGUSR1, &sig_handler);

    ncurses_window();

    pthread_join(t, NULL);
    globals_shutdown();

#ifdef ENABLE_LOGGING
    log_shitdown();
#endif


    return 0;
}

