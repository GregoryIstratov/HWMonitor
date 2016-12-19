#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <bits/mman.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>
#include <regex.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>


//external
#include <blkid/blkid.h> // apt install libblkid-dev
#include <ncurses.h>     // apt install libncurses5-dev
#include <stdbool.h>

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
#define LOG_ERROR(...) (_log(__FILE__, __LINE__, __func__, LOG_ERROR, __VA_ARGS__))
#define LOG_WARN(...) (_log(__FILE__, __LINE__, __func__, LOG_WARN, __VA_ARGS__))
#define LOG_DEBUG(...) (_log(__FILE__, __LINE__, __func__, LOG_DEBUG, __VA_ARGS__))
#define LOG_INFO(...) (_log(__FILE__, __LINE__, __func__, LOG_INFO, __VA_ARGS__))
#define LOG_TRACE(...) (_log(__FILE__, __LINE__, __func__, LOG_TRACE, __VA_ARGS__))
#define LOG_ASSERT(...) (_log(__FILE__, __LINE__, __func__, LOG_ASSERT, __VA_ARGS__))
#define ASSERT(exp) ((exp)?__ASSERT_VOID_CAST (0): _log(__FILE__, __LINE__, __func__, LOG_ASSERT, #exp))

#define ASSERT_EQ(a, b) ((a == b)?__ASSERT_VOID_CAST (0): LOG_ASSERT("%s != %s [%lu] != [%lu]", #a, #b, a, b))

//#define LOG_SHOW_TIME
//#define LOG_SHOW_DATE
//#define LOG_SHOW_THREAD
#define LOG_SHOW_PATH
//#define LOG_ENABLE_MULTITHREADING

#define LOG_FORMAT_BUFFER_MAX_SIZE 2048


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

static void init_log(int loglvl) {
    loglevel = loglvl;
#ifdef LOG_ENABLE_MULTITHREADING
    pthread_spin_init(&stderr_spinlock, 0);
    pthread_spin_init(&stdout_spinlock, 0);
#endif
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
        struct tm* tml;
        if(time(&t) == (time_t)-1)
        {
            LOG_ERROR("time return failed");
            return;
        }

        localtime_r(&t, &_tml);
        tml = &_tml;

#endif

        fprintf(stdout, "%s", log_color(lvl));
#ifdef LOG_SHOW_TIME
        fprintf(stdout, "[%02d:%02d:%02d]", tml->tm_hour, tml->tm_min, tml->tm_sec);
#endif
#ifdef LOG_SHOW_DATE
        fprintf(stdout, "[%02d/%02d/%d]", tml->tm_mday, tml->tm_mon + 1, tml->tm_year - 100);
#endif
#ifdef LOG_SHOW_THREAD
        fprintf(stdout, "[0x%08x]", tid);
#endif

        fprintf(stdout, "[%s][%s]: %s%s", loglevel_s(lvl), fun, buf, LOG_RESET);

#ifdef LOG_SHOW_PATH
        fprintf(stdout, " - %s:%d", file, line);
#endif
        fprintf(stdout, "\n");
        fflush(stdout);

#ifdef LOG_ENABLE_MULTITHREADING
        pthread_spin_unlock(&stdout_spinlock);
#endif

    }
}

#endif // ENABLE_LOGGING

//=======================================================================
// MACROSES
//=======================================================================

#define NULLPP(x) ((void**)x == ((void**)0))

#define PTR_TO_U64(ptr) (uint64_t)(uint64_t*)ptr

#define EXIT_ERROR(msg) { fprintf(stderr, "%s:%d: %s",  __FILE__, __LINE__, msg); exit(EXIT_FAILURE); }
//#define ASSERT(x) { if(!(#x)) { fprintf(stderr, "%s:%d [%s]: assertion failed [%s]",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #x); exit(EXIT_FAILURE); }}
//#define EXIT_IF_NULL(x) if(!(#x)) { fprintf(stderr, "%s:%d [%s]: returns NULL pointer",  __FILE__, __LINE__, __PRETTY_FUNCTION__); exit(EXIT_FAILURE); }

#define SAFE_RELEASE_PP(x) { if(x && *x){ safe_free((void**)x); *x = NULL; } }
#define SAFE_RELEASE(x) { if(x){ free(x); x = NULL; } }
//#define ERROR_EXIT() { perror(strerror(errno)); exit(errno); }

//===============================================================
#define STRING_INIT_BUFFER 4
#define ALLOC_ALIGN 16

#define HASHTABLE_SIZE 16

#define DA_MAX_MULTIPLICATOR 4

#define KiB 1024UL
#define MiB 1048576UL
#define GiB 1073741824UL
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

//======================================================================================================================
// RETURN MSG
//======================================================================================================================
// 64 bit
// [16 bit reserved][16 bit value data][16 bit idx msg][8 bit depth][8 bit code]


#define RET_MULTITHREADED

typedef uint64_t ret_t;

typedef struct ret_field
{
    union {
        struct{
            uint8_t code;
            uint8_t depth;
            uint16_t idx;
            uint16_t data;
            uint16_t reserved;
        };
        ret_t r;
    };

} ret_f_t;

pthread_spinlock_t gmsg_tab_lock;
static const char *gmsg_tab[UINT16_MAX]; // about 500 kb, //TODO make it dynamic
static uint16_t gmsg_idx = 1;

#define return_trace(ret) LOG_TRACE("[%016d][%016d][%08d][%016d][%08d]", ret.reserved, \
 ret.data, ret.depth, ret.idx, ret.code);


static ret_t return_create(uint8_t code) {

    ret_f_t ret = { .code = code };

    return ret.r;
}

static ret_t return_create_v(uint8_t code, uint16_t user_data) {
    ret_f_t ret = { .code =  code };
    ret.depth = 1;
    ret.data = user_data;

    return_trace(ret);

    return ret.r;
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
    ret.idx = gmsg_idx - 1;
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


static hashtable_t *alloc_table = NULL;

struct _IO_FILE *stdout_orig;
struct _IO_FILE *stderr_orig;
struct _IO_FILE *stdtrace;
static FILE *stdtest;
//static char* stdtest_buf;
//static size_t stdtest_size;
static const size_t size_npos = (size_t) -1;
// init gloabls

static void string_init_globals();

static ret_t init_gloabls() {

#ifdef RET_MULTITHREADED
    pthread_spin_init(&gmsg_tab_lock, 0);
#endif
    //disable buffering for stdout
    //setvbuf(stdout, NULL, _IONBF, 0);

#ifdef ENABLE_LOGGING
    init_log(LOGLEVEL_ALL);
#endif

    stdout_orig = stdout;
    stderr_orig = stderr;

    //ht_init(&alloc_table);

    //stdtest = open_memstream(&stdtest_buf, &stdtest_size);
    stdtest = fopen("/dev/null", "w");
    stdtrace = fopen("/dev/null", "w");

    string_init_globals();

    return 0;
}


static void enable_stdout(bool b) {
    if (b) stdout = stdout_orig;
    else
        stdout = stdtest;
}

static void enable_stderr(bool b) {
    if (b) stderr = stderr_orig;
    else
        stderr = stdtest;
}


static ret_t globals_shutdown() {

#ifdef RET_MULTITHREADED
    pthread_spin_destroy(&gmsg_tab_lock);
#endif

    //ht_destroy(alloc_table);
    fclose(stdtest);

    return ST_OK;
}


//======================================================================================================================
// blob operations
//======================================================================================================================

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

//===============================================================
// ALLOCATORS
//===============================================================

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

static void safe_free(void **pp) {
    if (NULLPP(pp) || *pp == NULL)
        return;


//    alloc_stat_t *stat = NULL;
//
//    if (_record_alloc_get(*pp, &stat) == ST_OK) {
//
//        _record_alloc_set(*pp, 0);
//
//        if(stat->size > 0) {
//            LOG_TRACE("found address: 0x%08lx size: %lu", (uint64_t) (uint64_t *) (*pp), stat->size);
//        }
//
//        free(stat);
//
//    }

    free(*pp);
    *pp = NULL;


}


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
        fprintf(stderr, "[ht_create_item] can't alloc");
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

    return ST_OK;
}

static ret_t da_init_n(dynamic_allocator_t **a, size_t size) {
    *a = zalloc(sizeof(dynamic_allocator_t));

    (*a)->ptr = zalloc(size);
    (*a)->size = size;
    (*a)->mul = 1;

    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              (*a), (*a)->ptr, (*a)->size, (*a)->used, (*a)->mul);

    return ST_OK;
}

static ret_t da_init(dynamic_allocator_t **a) {

    return da_init_n(a, STRING_INIT_BUFFER);
}

static ret_t da_release(dynamic_allocator_t **a) {
    if (NULLPP(a))
        return ST_EMPTY;

    if (*a) {
        LOG_TRACE("a[0x%08lX] size=%lu used=%lu mul=%lu",
                  (*a)->ptr, (*a)->size, (*a)->used, (*a)->mul);

        if ((*a)->ptr) free((*a)->ptr);
        safe_free((void **) a);
    }

    return ST_OK;
}

static ret_t da_fit(dynamic_allocator_t *a) {

    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);


    da_realloc(a, a->used);


    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return 0;
}

static ret_t da_crop_tail(dynamic_allocator_t *a, size_t pos) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
              a, a->ptr, a->size, a->used, a->mul, pos);

    if (pos > a->size) {
        LOG_WARN("a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
                 a, a->ptr, a->size, a->used, a->mul, pos);

        return ST_OUT_OF_RANGE;
    }

    a->size = a->used - pos;
    char *newbuff = zalloc(a->size);
    memcpy(newbuff, &a->ptr[pos], a->size);
    free(a->ptr);
    a->ptr = newbuff;
    a->used = a->size;

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return ST_OK;
}

static ret_t da_pop_head(dynamic_allocator_t *a, size_t n) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; n=%lu",
              a, a->ptr, a->size, a->used, a->mul, n);

    da_fit(a);
    if (n > a->size) {
        LOG_WARN("a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu",
                 a, a->ptr, a->size, a->used, a->mul, n);

        return ST_OUT_OF_RANGE;
    }

    da_realloc(a, a->size - n);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return ST_OK;
}

static ret_t da_check_size(dynamic_allocator_t *a, size_t new_size) {
    if (a->size < a->used + new_size)
        da_realloc(a, a->used + new_size);

    return 0;
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

    return ST_OK;
}

static ret_t da_sub2(dynamic_allocator_t *a, size_t begin, size_t end, dynamic_allocator_t **b) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; input begin=%lu end=%lu",
              a, a->ptr, a->size, a->used, a->mul, begin, end);

    size_t ssize = end - begin + 1;
    if (ssize > a->used) {

        LOG_WARN("a=[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; begin=%lu and end=%lu out of range",
                 a, a->ptr, a->size, a->used, a->mul, begin, end);

        return ST_OUT_OF_RANGE;
    }

    da_init_n(b, ssize);
    da_append(*b, a->ptr + begin - 1, ssize);

    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    LOG_TRACE("e b[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              (*b), (*b)->ptr, (*b)->size, (*b)->used, (*b)->mul);

    return ST_OK;

}

static ret_t da_sub(dynamic_allocator_t *a, size_t pos, dynamic_allocator_t **b) {
    return da_sub2(a, pos, a->used, b);
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

    return ST_OK;
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

    da_release(b);

    DA_TRACE(a);
    ASSERT(*b == NULL);

    return ST_OK;
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

    return ST_OK;
}

static ret_t da_remove_seq(dynamic_allocator_t *a, size_t pos, size_t n) {
    LOG_TRACE("b a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu; pos=%lu n=%lu",
              a, a->ptr, a->size, a->used, a->mul, n);


    if (pos + n > a->used) {
        LOG_WARN("pos(%lu) + n(%lu) >= total used(%lu) bytes", pos, n, a->used);
        return ST_SIZE_EXCEED;
    }

    dynamic_allocator_t *b = NULL;

    da_sub(a, pos + n, &b);

    da_realloc(a, pos - 1);

    da_merge(a, &b);

    ASSERT(b == NULL);
    LOG_TRACE("e a[0x%08lX] ptr=[0x%08lX] size=%lu used=%lu mul=%lu",
              a, a->ptr, a->size, a->used, a->mul);

    return ST_OK;

}

static ret_t da_compare(dynamic_allocator_t *a, dynamic_allocator_t *b) {
    return memcmp(a->ptr, b->ptr, MIN(a->used, b->used));
}

static void da_test() {
    dynamic_allocator_t *da = NULL;
    dynamic_allocator_t *db = NULL;
    dynamic_allocator_t *dc = NULL;
    dynamic_allocator_t *dd = NULL;
    dynamic_allocator_t *de = NULL;
    dynamic_allocator_t *df = NULL;

    da_init(&da);

    // 0    H
    // 2    e
    // 3    l
    // 4    l
    // 5    o
    // 6    ,
    // 7
    // 8    S
    // 9    w
    // 10   e
    // 11   e
    // 12   t
    // 13
    // 14   M
    // 15   a
    // 16   r
    // 17   i
    // 18   a
    // 19   !
    char const *str1 = "Hello, Sweet Maria!";
    da_append(da, str1, strlen(str1));

    da_sub2(da, 14, 19, &db);
    da_sub(da, 14, &dc);

    ASSERT(da_compare(db, db) == 0);

    da_init(&de);
    da_append(de, " My ", 4);
    da_sub2(da, 8, 12, &df);
    da_append(df, "!", 1);

    da_remove_seq(da, 8, 6);

    da_merge(de, &df);

    da_concant(da, de);


    da_release(&da);
    ASSERT(NULL == da);
    da_release(&db);
    ASSERT(NULL == db);
    da_release(&dc);
    ASSERT(NULL == dc);
    da_release(&dd);
    ASSERT(NULL == dd);
    da_release(&de);
    ASSERT(NULL == de);
    da_release(&df);
    ASSERT(NULL == df);
}

//=======================================================================
// GENERIC LIST
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
    size_t elem_size;

} list_t;


static void list_init(list_t **l, size_t elem_size) {
    *l = zalloc(sizeof(list_t));
    (*l)->elem_size = elem_size;
}

static void node_init(list_node_t **node) {
    *node = zalloc(sizeof(list_node_t));
}

static void *list_dub_data(list_t *l, void *data) {
    void *v = zalloc(l->elem_size);
    memcpy(v, data, l->elem_size);
    return v;
}

static void node_append(list_t *l, bool tail, list_node_t *node, list_node_t *prev, void *data) {
    if (!node)
        node_init(&node);

    if (node->data == NULL) {
        node->data = list_dub_data(l, data);
        node->prev = prev;

        if (prev) {
            if (tail)
                prev->next = node;
            else
                prev->prev = node;
        }

        l->size++;
        if (tail)
            l->tail = node;
        else
            l->head = node;
        return;
    }

    node_append(l, tail, node->next, node, data);

}

static void list_append_tail(list_t *l, void *s) {
    if (!l->head)
        node_init(&l->head);

    if (!l->tail)
        node_init(&l->tail);

    node_append(l, true, l->head, NULL, s);

}

static void list_append_head(list_t *l, void *s) {
    if (!l->head)
        node_init(&l->head);

    if (!l->tail)
        node_init(&l->tail);

    node_append(l, false, l->head, NULL, s);

}

static void *list_pop_head(list_t *l) {
    if (l->size == 0)
        return NULL;

    list_node_t *tmp = l->head;
    l->head = tmp->prev;
    l->size--;

    void *data = tmp->data;
    safe_free((void **) &tmp);

    return data;
}

static void *list_crop_tail(list_t *l) {
    if (l->size == 0)
        return NULL;

    list_node_t *tmp = l->tail;
    l->tail = tmp->next;
    l->size--;

    void *data = tmp->data;
    safe_free((void **) &tmp);

    return data;
}

static list_node_t *list_next(list_node_t *node) {
    if (!node) return NULL;

    return node->next;
}

static list_node_t *list_prev(list_node_t *node) {
    if (!node) return NULL;

    return node->prev;
}

static ret_t list_release(list_t **l) {
    if (NULLPP(l) && *l == NULL)
        return ST_EMPTY;


    list_node_t *head = (*l)->head;
    while (head) {

        head = head->next;

        list_node_t *tmp = head;
        safe_free((void **) &tmp);
    }

    safe_free((void **) l);
    *l = NULL;


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

static ret_t list_remove(list_t *l, void *data) {
    list_node_t *head = l->head;

    while (head) {
        if (head->data == data) {
            list_node_t *hn = head->next;
            list_node_t *hp = head->prev;

            hn->prev = hp;
            hp->next = hn;

            free(head);

            return ST_OK;

        }

        head = head->next;
    }

    return ST_NOT_FOUND;
}

typedef void(*list_traverse_cb)(list_node_t *);


static ret_t list_traverse(list_t *l, bool forward, list_traverse_cb cb) {
    if (!l) return ST_EMPTY;


    list_node_t *cur = forward ? l->tail : l->head;

    while (cur) {
        cb(cur);
        cur = forward ? cur->next : cur->prev;
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
    da_release(&vec->alloc);
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


//=======================================================================
// STRING
//=======================================================================

typedef struct {
    uint8_t _[sizeof(dynamic_allocator_t)];

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

static ret_t string_release(string **s) {
    return da_release(_dap(s));
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

static ret_t string_dub(string *s, string **ns) {
    return da_dub(_da(s), _dap(ns));
}

static ret_t string_append(string *s, const char *str) {
    size_t len = strlen(str);
    return da_append(_da(s), str, len);

}

static void string_init_globals() {
    string_init(&string_null);
    string_append(string_null, "(null)");
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

static ret_t string_starts_with(string *s, const char *str) {
    size_t str_len = strlen(str);
    if (str_len > string_size(s))
        return ST_SIZE_EXCEED;

    if (memcmp(string_cdata(s), str, str_len) == 0)
        return ST_OK;

    return ST_NOT_FOUND;
}

static ret_t string_compare(string *a, string *b) {
    return da_compare(_da(a), _da(b));
}

static ret_t string_comparez(string *a, const char *str) {
    return string_starts_with(a, str);
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

static void string_test() {
    static const char *str1 = "Hello, World!";
    static const char *str2 = "What's up, Dude?";
    ASSERT(sizeof(dynamic_allocator_t) == sizeof(string));

    string *a = NULL;
    string *b = NULL;
//    string* c = NULL;
//    string* d = NULL;


    ASSERT(string_init(&a) == ST_OK);
    ASSERT(string_release(&a) == ST_OK);

    ASSERT(string_create(&a, str1));
    ASSERT(string_comparez(a, str1));

    ASSERT(string_dub(a, &b) == ST_OK);
    ASSERT(string_compare(a, b) == 0);

    ASSERT(string_append(b, str2) == ST_OK);

    string_print(b);

    ASSERT(string_add(b, a) == ST_OK);

    string_printt(a);
    string_print(b);


}

//=======================================================================
// SLIST
//=======================================================================


typedef struct snode {
    string *s;
    struct snode *prev;
    struct snode *next;

} snode;

typedef struct {
    snode *head;
    snode *tail;
    size_t size;
    snode *cur;
    char delm;

} slist;


static void slist_init(slist **sl) {
    *sl = zalloc(sizeof(slist));
    (*sl)->delm = ',';
}

static void snode_init(snode **node) {
    *node = zalloc(sizeof(snode));
}


static void slist_set_delm(slist *sl, char delm) {
    sl->delm = delm;
}

static void snode_append(slist *sl, snode *node, snode *prev, string *s) {
    if (!node)
        snode_init(&node);

    if (node->s == NULL) {
        node->s = s;
        node->prev = prev;

        if (prev) prev->next = node;

        sl->size++;
        sl->tail = node;
        return;
    }

    snode_append(sl, node->next, node, s);

}

static void slist_append(slist *sl, string *s) {
    if (!sl->head)
        snode_init(&sl->head);

    if (!sl->tail)
        snode_init(&sl->tail);

    snode_append(sl, sl->head, NULL, s);

}

static string *slist_pop_head(slist *sl) {
    if (sl->size == 0) EXIT_ERROR("Empty list");

    snode *tmp = sl->head;
    sl->head = tmp->prev;
    sl->size--;

    string *s = tmp->s;
    free(tmp);

    return s;
}

static void slist_init_current(slist *sl) {
    if (sl->size == 0) EXIT_ERROR("Empty list");

    sl->cur = sl->head;
}

static string *slist_next(slist *sl) {
    if (sl->cur == NULL)
        return NULL;

    string *s = sl->cur->s;

    sl->cur = sl->cur->next;

    return s;
}

static ret_t slist_release(slist **sl, bool srelease) {
    if (sl && *sl) {
        snode *head = (*sl)->head;
        while (head) {
            if (srelease)
                string_release(&head->s);

            head = head->next;

            snode *tmp = head;
            safe_free((void **) &tmp);
        }

        safe_free((void **) sl);
    }

    return ST_OK;

}

static void slist_merge(slist *a, slist *b) {
    if (a->size == 0 && b->size == 0) EXIT_ERROR("lists must contain at least one element");


    a->tail->next = b->head;
    b->head->prev = a->tail;
    a->size += b->size;
    a->tail = b->tail;
}

static void slist_remove(slist *sl, string *s) {
    snode *head = sl->head;

    while (head) {
        if (head->s == s) {
            snode *hn = head->next;
            snode *hp = head->prev;

            hn->prev = hp;
            hp->next = hn;

            free(head);

            return;

        }

        head = head->next;
    }
}

static ret_t slist_find_eq(slist *sl, string *s, string **found) {
    snode *head = sl->head;

    while (head) {
        if (string_compare(head->s, s) == ST_OK) {
            *found = head->s;
            return ST_OK;
        }

        head = head->next;
    }

    return ST_NOT_FOUND;
}

static void slist_fprint(slist *sl, FILE *output) {
    snode *head = sl->head;
    while (head) {

#ifdef NDEBUG
        string_print(head->s);
#else
        string_printd(head->s);
#endif

        head = head->next;
    }
}

static void slist_rfprint(slist *sl, FILE *output) {
    snode *tail = sl->tail;

    while (tail) {
#ifdef NDEBUG
        string_print(tail->s);
#else
        string_printd(tail->s);
#endif
        tail = tail->prev;
    }
}

static ret_t string_remove_seq(string *s, size_t pos, size_t n) {
    return da_remove_seq(_da(s), pos, n);
}

static ret_t string_remove_dubseq(string *s, char delm) {
    size_t j = 0;
    while (j < string_size(s)) {
        const char *cur = string_cdata(s);
        size_t i = 0;
        size_t n = 0;

        while (cur[j + (i++)] == delm) ++n;


        if (n > 1) {
            int res = ST_OK;
            if ((res = string_remove_seq(s, j + i, n) != ST_OK))
                return res;

            j = 0; // skip due to internal buffer changed
        }

        ++j;

        if (j >= string_size(s))
            break;

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
    while (string_size(s) > 0) {
        size_t idx = string_size(s) - 1;
        char sch = string_char(s, idx);

        if (check_strip_dict(sch))
            string_pop_head(s, 1);
        else
            return ST_OK;

    }

    return ST_OK;
}

static ret_t string_lstrip(string *s) {
    for (size_t j = 0; j < string_size(s); ++j) {

        char sch = string_char(s, j);
        if (check_strip_dict(sch))
            string_crop_tail(s, 1);
        else
            return ST_OK;
    }
    return ST_OK;
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

static ret_t string_split(string *s, char delm, slist **sl) {
    if (string_size(s) == 0)
        return ST_EMPTY;


    string_rstrip_ws(s);
    string_remove_dubseq(s, delm);

    slist_init(sl);
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

            slist_append(*sl, ss);

            cb = ++ccur;
        }

        ++ccur;
    }

    return ST_OK;
}


//=============================================================================================
// HASH BINARY TREE
//=============================================================================================

typedef struct bt_node {
    uint64_t hash_key;
    void *data;
    size_t size;
    struct bt_node *prev;
    struct bt_node *left;
    struct bt_node *right;

} bt_node_t;

static ret_t bt_node_create(bt_node_t **bt, uint64_t hash, void *data, size_t size) {
    *bt = zalloc(sizeof(bt_node_t));
    bt_node_t *b = *bt;
    b->hash_key = hash;

    b->data = zalloc(size);
    memcpy(b->data, data, size);

    return ST_OK;
}

static ret_t bt_node_release(bt_node_t **bt) {

    if (NULLPP(bt))
        return ST_EMPTY;

    if (*bt != NULL) {

        bt_node_release(&(*bt)->left);
        bt_node_release(&(*bt)->right);

        safe_free(&(*bt)->data);
        safe_free((void **) bt);

    }

    return ST_OK;
}


static ret_t bt_node_set(bt_node_t **bt, bt_node_t *prev, uint64_t hash, void *data, size_t size) {
    if (*bt == NULL) {
        *bt = zalloc(sizeof(bt_node_t));
        bt_node_t *b = *bt;
        b->hash_key = hash;
        b->prev = prev;

        b->data = zalloc(size);
        memcpy(b->data, data, size);

        return ST_OK;
    } else {
        bt_node_t *b = *bt;

        if (b->hash_key > hash)
            return bt_node_set(&b->right, b, hash, data, size);
        else if (b->hash_key < hash)
            return bt_node_set(&b->left, b, hash, data, size);
        else {
            SAFE_RELEASE(b->data);

            b->data = zalloc(size);
            memcpy(b->data, data, size);

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
    size_t elem_size;
} binary_tree_t;


static ret_t bt_init(binary_tree_t **bt, size_t elem_size) {
    *bt = zalloc(sizeof(binary_tree_t));
    (*bt)->elem_size = elem_size;

    return ST_OK;
}

static ret_t bt_release(binary_tree_t **bt) {
    if (NULLPP(bt))
        return ST_EMPTY;

    if (*bt != NULL) {
        bt_node_release(&(*bt)->head);

        safe_free((void **) bt);
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
    str_int_t data;
    data.str = key;
    data.i = i;
    return bt_node_set(&bt->head, NULL, hash, &data, bt->elem_size);
}

static ret_t bt_si_get(binary_tree_t *bt, const char *key, str_int_t **i) {
    uint64_t hash = crc64s(key);
    void *p = NULL;
    int ret = bt_node_get(bt->head, hash, &p);
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


static ret_t test_slist();

static void test_hash_bt();

static void da_test();

static void string_test();

static void test_ret();


static void run_tests() {
    test_ret();
}

static void test_hash_bt() {
    binary_tree_t *bt;
    str_int_t *node = NULL;
    bt_init(&bt, sizeof(str_int_t));

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

    bt_release(&bt);

    ASSERT(NULLPP(bt));
}

static ret_t test_slist() {
    slist *sl = NULL;
    slist_init(&sl);

    string *s1 = NULL;
    string *s2 = NULL;
    string *s3 = NULL;
    string *s4 = NULL;
    string *s5 = NULL;
    string *s6 = NULL;
    string *s7 = NULL;
    string *s55 = NULL;


    string_create(&s1, "test1");
    string_create(&s2, "test2");
    string_create(&s3, "test3");
    string_create(&s4, "test4");
    string_create(&s5, "test5");
    string_create(&s6, "test6");
    string_create(&s7, "test7");
    string_create(&s55, "test5");


    slist_append(sl, s1);
    slist_append(sl, s2);
    slist_append(sl, s3);
    slist_append(sl, s4);
    slist_append(sl, s5);
    slist_append(sl, s6);
    slist_append(sl, s7);


    slist_fprint(sl, stdtest);
    fprintf(stdtest, "\n\n");
    slist_rfprint(sl, stdtest);
    fprintf(stdtest, "\n\n");


    string *found = NULL;
    slist_find_eq(sl, s55, &found);


    slist_remove(sl, found);

    slist_fprint(sl, stdtest);
    fprintf(stdtest, "\n\n");
    slist_rfprint(sl, stdtest);
    fprintf(stdtest, "\n\n");


    slist *sl2 = NULL;
    slist_init(&sl2);

    string *s21 = NULL;
    string *s22 = NULL;
    string *s23 = NULL;
    string *s24 = NULL;
    string *s25 = NULL;
    string *s26 = NULL;
    string *s27 = NULL;


    string_create(&s21, "Ivan");
    string_create(&s22, "Lena");
    string_create(&s23, "Carina");
    string_create(&s24, "Dima");
    string_create(&s25, "Misha");
    string_create(&s26, "Andrey");
    string_create(&s27, "Katy");

    slist_append(sl2, s21);
    slist_append(sl2, s22);
    slist_append(sl2, s23);
    slist_append(sl2, s24);
    slist_append(sl2, s25);
    slist_append(sl2, s26);
    slist_append(sl2, s27);


    slist_fprint(sl2, stdtest);
    fprintf(stdtest, "\n\n");
    slist_rfprint(sl2, stdtest);
    fprintf(stdtest, "\n\n");


    slist_merge(sl, sl2);

    slist_fprint(sl, stdtest);
    fprintf(stdtest, "\n\n");
    slist_rfprint(sl, stdtest);
    fprintf(stdtest, "\n\n");


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


    slist *blklist = NULL;
    string_split(text, '\n', &blklist);

    fprintf(stdout, "------- LINES OF TOKENS -----------\n");
    slist_fprint(blklist, stdout);
    fflush(stdout);

    string *tk = NULL;
    slist_init_current(blklist);
    while ((tk = slist_next(blklist)) != NULL) {

        fprintf(stdout, "------- TOKEN LINE-----------\n");
        string_printd(tk);

        fprintf(stdout, "------- TOKEN SPLIT-----------\n");

        slist *tokens = NULL;
        string_split(tk, ' ', &tokens);

        slist_fprint(tokens, stdout);

        slist_init_current(tokens);

        string *s = NULL;
        while ((s = slist_next(tokens)) != NULL) {
            slist *kv = NULL;
            string_split(s, '=', &kv);

            fprintf(stdout, "------- TOKEN KEY-VALUE -----------\n");
            slist_fprint(kv, stdout);


            slist_init_current(kv);
            string *key = slist_next(kv);
            string *val = slist_next(kv);

            string_strip(val);

            fprintf(stdout, "------- STRIPED TOKEN KEY-VALUE -----------\n");
            string_printd(key);
            string_printd(string_null);

            slist_release(&kv, true);
        }

        slist_release(&tokens, true);

    }


    {
        slist_release(&sl, true);
        slist_release(&sl2, true);
        slist_release(&blklist, true);
        string_release(&text);
    }

    return ST_OK;

}

//========================================================================


enum {
    /// These values increment when an I/O request completes.
            READ_IO = 0, ///requests - number of read I/Os processed

    /// These values increment when an I/O request is merged with an already-queued I/O request.
            READ_MERGE, /// requests - number of read I/Os merged with in-queue I/O

    /// These values count the number of sectors read from or written to this block device.
    /// The "sectors" in question are the standard UNIX 512-byte sectors, not any device- or
    /// filesystem-specific block size.
    /// The counters are incremented when the I/O completes.
            READ_SECTORS, /// requests - number of read I/Os merged with in-queue I/O


    /// These values count the number of milliseconds that I/O requests have
    /// waited on this block device.  If there are multiple I/O requests waiting,
    /// these values will increase at a rate greater than 1000/second; for
    /// example, if 60 read requests wait for an average of 30 ms, the read_ticks
    /// field will increase by 60*30 = 1800.
            READ_TICKS, ///milliseconds - total wait time for read requests

    /// These values increment when an I/O request completes.
            WRITE_IO, /// requests - number of write I/Os processed

    /// These values increment when an I/O request is merged with an already-queued I/O request.
            WRITE_MERGES, /// requests - number of write I/Os merged with in-queue I/O

    /// These values count the number of sectors read from or written to this block device.
    /// The "sectors" in question are the standard UNIX 512-byte sectors, not any device- or
    /// filesystem-specific block size.
    /// The counters are incremented when the I/O completes.
            WRITE_SECTORS, /// sectors - number of sectors written


    /// These values count the number of milliseconds that I/O requests have
    /// waited on this block device.  If there are multiple I/O requests waiting,
    /// these values will increase at a rate greater than 1000/second; for
    /// example, if 60 read requests wait for an average of 30 ms, the read_ticks
    /// field will increase by 60*30 = 1800.
            WRITE_TICKS, /// milliseconds - total wait time for write requests

    /// This value counts the number of I/O requests that have been issued to
    /// the device driver but have not yet completed.  It does not include I/O
    /// requests that are in the queue but not yet issued to the device driver.
            IN_FLIGHT, /// requests - number of I/Os currently in flight

    /// This value counts the number of milliseconds during which the device has
    /// had I/O requests queued.
            IO_TICKS, /// milliseconds - total time this block device has been active

    /// This value counts the number of milliseconds that I/O requests have waited
    /// on this block device.  If there are multiple I/O requests waiting, this
    /// value will increase as the product of the number of milliseconds times the
    /// number of requests waiting (see "read ticks" above for an example).
            TIME_IN_QUEUE /// milliseconds - total wait time for all requests
};


static size_t get_sfile_size(const char *filename) {
    struct stat st;
    stat(filename, &st);
    return (size_t) st.st_size;
}

typedef void(*cmd_exec_cb)(void *, slist *);

static void sfile_mmap(const char *filename, string *s) {
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

static ret_t cmd_execute(const char *cmd, void *ctx, cmd_exec_cb cb) {
    FILE *fpipe;

    if (!(fpipe = popen(cmd, "r")))
        return ST_ERR;

    char line[1024] = {0};


    slist *sl;
    slist_init(&sl);

    while (fgets(line, sizeof(line), fpipe)) {
        string *s = NULL;

        string_create(&s, line);
        slist_append(sl, s);
    }

    cb(ctx, sl);

    pclose(fpipe);

    return ST_OK;
}


enum {
    DFS_TOTAL = 0,
    DFS_USED = 1,
    DFS_AVAIL = 2,
    DFS_USE = 3

};


typedef struct df_stat {
    string *dev;

    uint64_t total;
    uint64_t used;
    uint64_t avail;
    uint64_t use;


} df_stat_t;

typedef struct {
    hashtable_t *stats;
    uint64_t skip_first;
} df_t;


static void df_init(df_t **df) {
    *df = zalloc(sizeof(df_t));
    (*df)->stats = zalloc(sizeof(hashtable_t));
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

static void df_callback(void *ctx, slist *lines) {
    df_t *df = (df_t *) ctx;

    fprintf(stdout, "------- LINES OF TOKENS -----------\n");
    slist_fprint(lines, stdout);
    fflush(stdout);

    string *tk = NULL;
    slist_init_current(lines);
    while ((tk = slist_next(lines)) != NULL) {

        if (string_starts_with(tk, "/dev/") != ST_OK)
            continue;

        fprintf(stdout, "------- TOKEN LINE-----------\n");
        string_print(tk);

        fprintf(stdout, "------- TOKEN SPLIT-----------\n");

        slist *tokens = NULL;
        string_split(tk, ' ', &tokens);

        slist_fprint(tokens, stdout);

        slist_init_current(tokens);

        //==============================
        // init df_stat

        df_stat_t *stat = zalloc(sizeof(df_stat_t));


        //==============================


        string *s = NULL;
        string *sname = NULL;
        uint64_t k = 0;
        while ((s = slist_next(tokens)) != NULL) {

            string_strip(s);

            switch (k) {
                case 0:
                    string_dub(s, &sname);
                    string_dub(s, &stat->dev);
                    break;
                case 1:
                    string_to_u64(s, &stat->total);
                    break;
                case 2:
                    string_to_u64(s, &stat->used);
                    break;
                case 3:
                    string_to_u64(s, &stat->avail);
                    break;
                case 4: {
                    string_pop_head(s, 1);
                    string_to_u64(s, &stat->use);
                    break;
                }

                default:
                    break;
            }

            ++k;

        }


        //add to ht, not need to free
        ht_set_df(df->stats, sname, stat);

        string_release(&sname);

        slist_release(&tokens, true);

    }


}

static void df_execute(df_t *dfs) {

    string *s;
    string_create(&s, "df --block-size=1");

    char *cmd = NULL;
    char *cmd_end;
    string_map_string(s, &cmd, &cmd_end);
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
    hashtable_t *blk;
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

#define MAX_ERROR_MSG 0x1000

/* Compile the regular expression described by "regex_text" into
   "r". */

static ret_t compile_regex(regex_t *r, const char *regex_text) {
    int status = regcomp(r, regex_text, REG_EXTENDED | REG_NEWLINE);
    if (status != 0) {
        char error_message[MAX_ERROR_MSG];
        regerror(status, r, error_message, MAX_ERROR_MSG);
        fprintf(stderr, "Regex error compiling '%s': %s\n",
                regex_text, error_message);
        return 1;
    }
    return 0;
}

/*
  Match the string in "to_match" against the compiled regular
  expression in "r".
 */

static ret_t match_regex(regex_t *r, const char *to_match) {
    /* "P" is a pointer into the string which points to the end of the
       previous match. */
    const char *p = to_match;
    /* "N_matches" is the maximum number of matches allowed. */
    const int n_matches = 10;
    /* "M" contains the matches found. */
    regmatch_t m[n_matches];

    while (1) {
        int i = 0;
        int nomatch = regexec(r, p, n_matches, m, 0);
        if (nomatch) {
            printf("No more matches.\n");
            return nomatch;
        }
        for (i = 0; i < n_matches; i++) {
            int start;
            int finish;
            if (m[i].rm_so == -1) {
                break;
            }
            start = (int) (m[i].rm_so + (p - to_match));
            finish = (int) (m[i].rm_eo + (p - to_match));
            if (i == 0) {
                printf("$& is ");
            } else {
                printf("$%d is ", i);
            }
            printf("'%.*s' (bytes %d:%d)\n", (finish - start),
                   to_match + start, start, finish);
        }
        p += m[0].rm_eo;
    }
    return 0;
}


static void sblk_callback(void *ctx, slist *lines) {

    sblkid_t *sys = (sblkid_t *) ctx;
    ht_init(&sys->blk);

    {
        string *tk = NULL;
        slist_init_current(lines);
        while ((tk = slist_next(lines)) != NULL) {

            fprintf(stdout, "------- TOKEN LINE-----------\n");
            string_print(tk);


            //================================
            regex_t re;
            compile_regex(&re, "(\\w+)=\"([[:alnum:][:space:]-]*)\"");

            char *tkp = NULL;
            string_create_nt(tk, &tkp);

            match_regex(&re, tkp);

            free(tkp);

            //================================
        }
    }

    return;

    fprintf(stdout, "------- LINES OF TOKENS -----------\n");
    slist_fprint(lines, stdout);
    fflush(stdout);

    string *tk = NULL;
    slist_init_current(lines);
    while ((tk = slist_next(lines)) != NULL) {

        fprintf(stdout, "------- TOKEN LINE-----------\n");
        string_print(tk);

        fprintf(stdout, "------- TOKEN SPLIT-----------\n");

        slist *tokens = NULL;
        string_split(tk, ' ', &tokens);


        slist_fprint(tokens, stdout);

        slist_init_current(tokens);

        //==============================
        // vector pair init

        vector_t *vec = NULL;
        vector_init(&vec, sizeof(skey_value_t));

        //==============================


        string *s = NULL;
        string *sname = NULL;
        while ((s = slist_next(tokens)) != NULL) {


            slist *kv = NULL;
            string_split(s, '=', &kv);

            fprintf(stdout, "------- TOKEN KEY-VALUE -----------\n");
            slist_fprint(kv, stdout);


            slist_init_current(kv);
            string *key = slist_next(kv);
            string *val = slist_next(kv);

            string_strip(val);

            if (string_comparez(key, "NAME") == ST_OK)
                string_dub(val, &sname);

            fprintf(stdout, "------- STRIPED TOKEN KEY-VALUE -----------\n");

            //add kv
            vector_add_kv(vec, key, val);

            string_print(key);
            string_print(val);

            slist_release(&kv, true);
        }


        //add to ht, not need to free
        ht_set_s(sys->blk, sname, vec);

        string_release(&sname);

        slist_release(&tokens, true);

    }

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


    char *ccmd = NULL;
    char *cmd_end = NULL;
    string_map_string(cmd, &ccmd, &cmd_end);
    cmd_execute(ccmd, sblk, &sblk_callback);

    return ST_OK;
}


typedef struct device {
    string *name;
    //struct statvfs stats;
    //std::vector <uint64_t> stat;
    string *perf_read;
    string *perf_write;
    string *label;
    uint64_t size;
    uint64_t used;
    uint64_t avail;
    double perc;
    string *fsize;
    string *fuse;
    string *fs;
    string *mount;
    string *sysfolder;
    string *model;
    uint64_t child;
    vector_t childs;
} device_t;

//static ret_t read_all_file(const char* path) {
//    FILE* f;
//
//    if(!(f = fopen(path, "r"))
//        return ST_ERR;
//
//
//
//
//    fclose(f);
//
//    return ST_OK;
//}
//
//static std::vector<std::string> scan_dir(std::string basedir, std::regex re) {
//    dirent *dir = nullptr;
//    DIR *d = opendir(basedir.c_str());
//
//    std::vector<std::string> v;
//    if (d) {
//        while ((dir = readdir(d)) != NULL) {
//            if (std::regex_match(dir->d_name, re)) {
//                std::stringstream ss;
//                ss << basedir << "/" << std::string(dir->d_name);
//                v.push_back(ss.str());
//            }
//        }
//
//        closedir(d);
//    }
//
//    return v;
//}



//std::string hr_size(uint64_t bytes) {
//
//    double r = bytes / 1024.;
//    if (r < 1024)  // KB / sec
//        return tostring((uint64_t) r) + "Kb";
//
//    r = bytes / 1024 / 1024;
//    if (r < 1024)  // MiB / sec
//        return tostring((uint64_t) r) + "Mb";
//
//    r = bytes / 1024 / 1024 / 1024;
//    return tostring((uint64_t) r) + "Gb";
//}

//
//class DeviceManager {
//    std::vector<device_ptr> base_devs;
//public:
//    void detect() {
//        std::regex re1("^s.*$");
//        auto vdev = scan_dir("/sys/block", re1);
//
//        base_devs.resize(vdev.size());
//
//        for (auto &dev : base_devs)
//            dev = std::make_shared<Device>();
//
//        for (size_t i = 0; i < vdev.size(); ++i) {
//            auto pos = vdev[i].find_last_of('/');
//            auto devn = vdev[i].substr(pos + 1);
//
//
//            base_devs[i]->sysfolder = vdev[i];
//            base_devs[i]->name = devn;
//            base_devs[i]->child = false;
//
//            auto childsv = scan_dir(base_devs[i]->sysfolder, std::regex(base_devs[i]->name + "[1-9]+$"));
//
//            for (size_t j = 0; j < childsv.size(); ++j) {
//                device_ptr child = std::make_shared<Device>();
//                child->sysfolder = childsv[j];
//                auto cpos = childsv[j].find_last_of('/');
//                auto cdevn = childsv[j].substr(cpos + 1);
//                child->name = cdevn;
//                child->child = true;
//
//                base_devs[i]->childs.push_back(child);
//            }
//
//
//        }
//    }
//
//    void enrich_devs() {
//        for (auto dev : base_devs) {
//            for (auto cdev : dev->childs) {
//                enrich(cdev);
//            }
//
//            enrich(dev);
//
//
//        }
//    }
//
//    std::vector<device_ptr> devs() {
//        return base_devs;
//    }
//
//private:
//
//    device_ptr get_dev_by_name(device_ptr base, const std::string &name) {
//        if (base->name == name)
//            return base;
//        else if (base->childs.size() > 0) {
//            for (size_t i = 0; i < base->childs.size(); ++i) {
//                if (get_dev_by_name(base->childs[i], name))
//                    return base->childs[i];
//            }
//        }
//
//        return nullptr;
//    }
//
//    int enrich(device_ptr dev) {
//        enrich_dev_stat(dev);
//        enrich_etc(dev);
//        enrich_size(dev);
//
//        return ST_OK;
//    }
//
//    int enrich_etc(device_ptr dev) {
//        sblkid_t blk;
//        blk.exec("/dev/" + dev->name);
//        dev->fs = blk.stat("FSTYPE");
//        dev->model = blk.stat("MODEL");
//        dev->mount = blk.stat("MOUNTPOINT");
//        dev->uuid = blk.stat("UUID");
//        dev->label = blk.stat("LABEL");
//
//        if (!dev->child) {
//            dev->size = std::stoull(blk.stat("SIZE"));
//            //dev->fsize = hr_size(dev->size);
//        }
//
//        return ST_OK;
//    }
//
//    int enrich_size(device_ptr dev) {
//        df_t df;
//        df.exec("/dev/" + dev->name);
//
//        if (dev->child) {
//            dev->size = df.total();
//            dev->used = df.used();
//            dev->avail = df.avail();
//            dev->perc = dev->used / (double) dev->size * 100.0;
//        } else {
//            for (auto ch : dev->childs) {
//                dev->used += ch->used;
//            }
//
//
//            dev->perc = dev->used / (double) dev->size * 100.0;
//        }
//
//
//        auto hr_used = hr_size(dev->used);
//        auto hr_total = hr_size(dev->size);
//        auto hr_use = f2s(dev->perc, 2);
//
//        std::stringstream ss;
//        ss << hr_used << "/" << hr_total;
//
//        dev->fuse = tostring(hr_use) + "%";
//
//        dev->fsize = ss.str();
//
//        return ST_OK;
//    }
//
//    int enrich_dev_stat(device_ptr dev) {
//        auto filename = dev->sysfolder + "/stat";
//        auto data = read_all_file(filename);
//
//        data = data.substr(0, data.find_last_of('\n'));
//
//        std::regex re("([0-9]+)");
//
//        std::vector<uint64_t> v;
//        for (auto it = std::sregex_iterator(data.begin(), data.end(), re);
//             it != std::sregex_iterator(); ++it) {
//            std::smatch m = *it;
//            uint64_t i = std::stoull(m.str());
//            v.push_back(i);
//
//        }
//
//        dev->stat = std::move(v);
//
//        return ST_OK;
//    }
//};
//
//class PerfMeter {
//
//    uint32_t sample_size_;
//    const size_t BLOCK_SIZE = 512; // Unix block size
//public:
//    PerfMeter(uint32_t sample_size)
//            : sample_size_(sample_size) {
//    }
//
//    uint32_t sample_size() const { return sample_size_; }
//
//    std::vector<device_ptr> measure() {
//        DeviceManager dm1;
//        dm1.detect();
//        dm1.enrich_devs();
//
//        std::this_thread::sleep_for(std::chrono::seconds(sample_size_));
//
//        DeviceManager dm2;
//        dm2.detect();
//        dm2.enrich_devs();
//
//        std::vector<device_ptr> devs1 = dm1.devs();
//        std::vector<device_ptr> devs2 = dm2.devs();
//
//        if (devs1.size() != devs2.size())
//            std::cerr << "Integrity of devices is corrupted" << std::endl;
//
//        std::vector<device_ptr> devs3;
//        devs3.resize(devs2.size());
//        for (size_t i = 0; i < devs2.size(); ++i) {
//            devs3[i] = diff(devs1[i], devs2[i]);
//        }
//
//        return devs3;
//    }
//
//
//private:
//    std::string human_readable(uint64_t bytes) {
//
//        float r = bytes / 1024.f / sample_size_;
//        if (r < 1024)  // KB / sec
//            return f2s(r, 3) + " Kb/s";
//
//        r = bytes / 1024 / 1024 / sample_size_;
//        if (r < 1024)  // MiB / sec
//            return f2s(r, 3) + " Mb/s";
//
//        r = bytes / 1024 / 1024 / 1024 / sample_size_;
//        return f2s(r, 3) + " Gb/s";
//    }
//
//    inline device_ptr diff(device_ptr a, device_ptr b) {
//        auto c = b->deepcopy();
//        c->stat[WRITE_SECTORS] = b->stat[WRITE_SECTORS] - a->stat[WRITE_SECTORS];
//        c->stat[READ_SECTORS] = b->stat[READ_SECTORS] - a->stat[READ_SECTORS];
//        c->perf["READ"] = human_readable(c->stat[READ_SECTORS] * BLOCK_SIZE);
//        c->perf["WRITE"] = human_readable(c->stat[WRITE_SECTORS] * BLOCK_SIZE);
//
//        for (size_t i = 0; i < b->childs.size(); ++i) {
//            auto cdev = c->childs[i];
//            auto adev = a->childs[i];
//            cdev->stat[WRITE_SECTORS] = cdev->stat[WRITE_SECTORS] - adev->stat[WRITE_SECTORS];
//            cdev->stat[READ_SECTORS] = cdev->stat[READ_SECTORS] - adev->stat[READ_SECTORS];
//            cdev->perf["READ"] = human_readable(cdev->stat[READ_SECTORS] * BLOCK_SIZE);
//            cdev->perf["WRITE"] = human_readable(cdev->stat[WRITE_SECTORS] * BLOCK_SIZE);
//
//        }
//
//        return c;
//
//    }
//};
//
//static ret_t statfs_dev() {
//    const char *dev = "/usr/bin/gcc";
//    struct statvfs64 fs;
//
//    int res = statvfs64(dev, &fs);
//
//    const uint64_t total = fs.f_blocks * fs.f_frsize;
//    const uint64_t available = fs.f_bfree * fs.f_frsize;
//    const uint64_t used = total - available;
//    const double usedPercentage = ceil(used / total * 100.0);
//
//
//    std::cout << dev << " " << std::fixed << total << " " << used << " " << available << " " << usedPercentage << "%"
//              << std::endl;
//
//    return 0;
//}
//
//
//struct Colon {
//    static const int OFFSET = 1;
//    static const int DEVICE = OFFSET;
//    static const int READ = 9 + OFFSET;
//    static const int WRITE = 22 + OFFSET;
//    static const int SIZE = 35 + OFFSET;
//    static const int USE = 50 + OFFSET;
//    static const int FILESYSTEM = 58 + OFFSET;
//    static const int MOUNT = 64 + OFFSET;
//    static const int MODEL = 80 + OFFSET;
//};
//
//class Row {
//    int row = 4;
//
//public:
//    int operator++(int) {
//        return row++;
//    }
//
//    int operator++() {
//        return ++row;
//    }
//
//    operator int() {
//        return row;
//    }
//};
//
//int ncurses_windows() {
//    int mrow = 0, mcol = 0;
//    initscr();            /* Start curses mode 		  */
//
//    if (has_colors() == FALSE) {
//        endwin();
//        printf("Your terminal does not support color\n");
//        exit(1);
//    }
//
//    //noecho();
//    //cbreak();
//    start_color();
//
//    init_pair(1, COLOR_WHITE, COLOR_BLACK);
//    init_pair(2, COLOR_GREEN, COLOR_BLACK);
//    init_pair(3, COLOR_CYAN, COLOR_BLACK);
//
//    getmaxyx(stdscr, mrow, mcol);
//
//    while (1) {
//        Row row;
//
//        PerfMeter pm(1);
//
//        auto devs = pm.measure();
//        clear();
//
//        attron(A_BOLD);
//        attron(COLOR_PAIR(1));
//
//        mvaddstr(1, 1, "HWMonitor 0.1a\n");
//
//        std::stringstream ssize;
//        ssize << "Sample size " << pm.sample_size() << "\n\n";
//        mvaddstr(2, 1, ssize.str().c_str());
//
//
//        mvaddstr(row, Colon::DEVICE, "Device");
//        mvaddstr(row, Colon::READ, "Read");
//        mvaddstr(row, Colon::WRITE, "Write");
//        mvaddstr(row, Colon::SIZE, "Size");
//        mvaddstr(row, Colon::USE, "Use");
//        mvaddstr(row, Colon::FILESYSTEM, "FS");
//        mvaddstr(row, Colon::MOUNT, "Mount");
//        mvaddstr(row++, Colon::MODEL, "Model");
//        attroff(COLOR_PAIR(1));
//
//
//        for (auto dev : devs) {
//            attron(COLOR_PAIR(2));
//
//            mvaddstr(row, Colon::DEVICE, dev->name.c_str());
//            mvaddstr(row, Colon::READ, dev->perf["READ"].c_str());
//            mvaddstr(row, Colon::WRITE, dev->perf["WRITE"].c_str());
//            mvaddstr(row, Colon::SIZE, dev->fsize.c_str());
//            mvaddstr(row, Colon::USE, dev->fuse.c_str());
//            mvaddstr(row, Colon::FILESYSTEM, dev->fs.c_str());
//            mvaddstr(row, Colon::MOUNT, dev->mount.c_str());
//            mvaddstr(row++, Colon::MODEL, dev->model.c_str());
//
//            attroff(COLOR_PAIR(2));
//            attron(COLOR_PAIR(3));
//            for (auto child : dev->childs) {
//                mvaddstr(row, Colon::DEVICE, child->name.c_str());
//                mvaddstr(row, Colon::READ, child->perf["READ"].c_str());
//                mvaddstr(row, Colon::WRITE, child->perf["WRITE"].c_str());
//                mvaddstr(row, Colon::SIZE, child->fsize.c_str());
//                mvaddstr(row, Colon::USE, child->fuse.c_str());
//                mvaddstr(row, Colon::FILESYSTEM, child->fs.c_str());
//                mvaddstr(row++, Colon::MOUNT, child->mount.c_str());
//            }
//
//            attroff(COLOR_PAIR(3));
//        }
//
//        attroff(A_BOLD);
//
//        refresh();            /* Print it on to the real screen */
//    }
//
//    getch();
//    endwin();
//}
//
//
//enum class PoArg {
//    NO_NCURSES,
//    DEBUG,
//    NET
//};
//
//
//std::map<PoArg, std::string> po_arg_parse(int argc, char **argv) {
//    std::map<PoArg, std::string> opts;
//
//    std::regex re("([tdn])");
//    std::string args;
//    for (int i = 1; i < argc; ++i)
//        args += std::string(argv[i]) + " ";
//
//    for (auto it = std::sregex_iterator(args.begin(), args.end(), re);
//         it != std::sregex_iterator();
//         ++it) {
//        std::smatch m = *it;
//        if (m.str() == "t") opts[PoArg::NO_NCURSES] = "";
//        if (m.str() == "d") opts[PoArg::DEBUG] = "";
//        if (m.str() == "n") opts[PoArg::NET] = "";
//    }
//
//    return opts;
//}
//
//int text_windows() {
//    for(;;) {
//        PerfMeter pm(1);
//
//        auto devs = pm.measure();
//
//
//        for (auto dev : devs) {
//
//            std::cout << dev->name << "\t" <<
//                      dev->perf["READ"] << "\t" <<
//                      dev->perf["WRITE"] << "\t" <<
//                      dev->fsize << '\t' <<
//                      dev->fuse << '\t' <<
//                      dev->fs << '\t' <<
//                      dev->mount << '\t' <<
//                      dev->model << std::endl;
//
//            for (auto child : dev->childs) {
//
//                std::cout << child->name << "\t" <<
//                          child->perf["READ"] << "\t" <<
//                          child->perf["WRITE"] << "\t" <<
//                          child->fsize << '\t' <<
//                          child->fuse << '\t' <<
//                          child->fs << '\t' <<
//                          child->mount << '\t' <<
//                          child->model << std::endl;
//            }
//
//        }
//
//    }
//}

struct A {
    OBJECT_DECLARE()

    int i;
    char ch;

    char *str;

};

static uint64_t total_leak = 0;

void scan_alloc(uint64_t hash, ht_key_t *key, ht_value_t *val) {
    fprintf(stderr, "[scan_alloc]: [0x%08lx] [0x%08lx] %lu bytes\n", hash, (uint64_t) val->ptr, val->size);
    total_leak += val->size;
    key->u.i = hash;
}


void scan_blk(uint64_t hash, ht_key_t *key, ht_value_t *val) {
    vector_t *vec = (vector_t *) val->ptr;

    size_t sz = vector_size(vec);

    for (size_t i = 0; i < sz; ++i) {
        skey_value_t *kv = NULL;
        vector_get(vec, i, (void **) &kv);
        fprintf(stderr, "[scan_blk][0x%08lx] %.*s=%.*s\n", hash, (int) string_size(kv->key), string_cdata(kv->key),
                (int) string_size(kv->value), string_cdata(kv->value));
    }


    key->u.i = hash;
}


void scan_df(uint64_t hash, ht_key_t *key, ht_value_t *val) {
    df_stat_t *stat = (df_stat_t *) val->ptr;


    fprintf(stderr, "[scan_df][0x%08lx][%.*s] SIZE=%lu USED=%lu AVAIL=%lu USE=%lu%% \n", hash,
            (int) string_size(stat->dev), string_cdata(stat->dev),
            stat->total, stat->used, stat->avail, stat->use);


    key->u.i = hash;
}

void check_style_defines() {
#ifdef _POSIX_SOURCE
    printf("_POSIX_SOURCE defined\n");
#endif

#ifdef _POSIX_C_SOURCE
    printf("_POSIX_C_SOURCE defined: %ldL\n", (long) _POSIX_C_SOURCE);
#endif

#ifdef _ISOC99_SOURCE
    printf("_ISOC99_SOURCE defined\n");
#endif

#ifdef _ISOC11_SOURCE
    printf("_ISOC11_SOURCE defined\n");
#endif

#ifdef _XOPEN_SOURCE
    printf("_XOPEN_SOURCE defined: %d\n", _XOPEN_SOURCE);
#endif

#ifdef _XOPEN_SOURCE_EXTENDED
    printf("_XOPEN_SOURCE_EXTENDED defined\n");
#endif

#ifdef _LARGEFILE64_SOURCE
    printf("_LARGEFILE64_SOURCE defined\n");
#endif

#ifdef _FILE_OFFSET_BITS
    printf("_FILE_OFFSET_BITS defined: %d\n", _FILE_OFFSET_BITS);
#endif

#ifdef _BSD_SOURCE
    printf("_BSD_SOURCE defined\n");
#endif

#ifdef _SVID_SOURCE
    printf("_SVID_SOURCE defined\n");
#endif

#ifdef _ATFILE_SOURCE
    printf("_ATFILE_SOURCE defined\n");
#endif

#ifdef _GNU_SOURCE
    printf("_GNU_SOURCE defined\n");
#endif

#ifdef _REENTRANT
    printf("_REENTRANT defined\n");
#endif

#ifdef _THREAD_SAFE
    printf("_THREAD_SAFE defined\n");
#endif

#ifdef _FORTIFY_SOURCE
    printf("_FORTIFY_SOURCE defined\n");
#endif
}

//=======================================================================================
// MAIN
//=======================================================================================

int main() {

    check_style_defines();
    init_gloabls();

    enable_stdout(true);
    enable_stderr(true);


    run_tests();

//
//    struct A* a = OBJECT_CREATE(struct A);
//    OBJECT_INIT(a);
//
//
//    a->ch = '\n';
//    a->i = 210490;
//    a->str = "Hello, World";
//
//    {
//        struct A *b = OBJECT_SHARE(a, struct A);
//        b->ch = 0;
//    }
//
//    OBJECT_RELEASE(a, struct A);
//


//    for(;;) {
//        test_slist();
//        usleep(1);
//
//    }




//    df_t* df;
//    df_init(&df);
//    df_execute(df);
//    ht_foreach(df->stats, &scan_df);



//    sblkid_t blk;
//    sblk_execute(&blk);
//
//    ht_foreach(blk.blk, &scan_blk);


    //ht_foreach(alloc_table, &scan_alloc);

    //fprintf(stderr, "Total leaked: %lu bytes\n", total_leak);

    globals_shutdown();

    return 0;
}

