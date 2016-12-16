// macro settings


//posix

#include <fcntl.h>
#include <malloc.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/param.h> // max/min
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <blkid/blkid.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/statvfs.h>

// c standart headers
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <math.h>
#include <stdalign.h>

#include <ncurses.h>
#include <bits/mman.h>


#define PTR_TO_U64(ptr) (uint64_t)(uint64_t*)ptr

#define EXIT_ERROR(msg) { fprintf(stderr, "%s:%d: %s",  __FILE__, __LINE__, msg); exit(EXIT_FAILURE); }
//#define ASSERT(x) { if(!(#x)) { fprintf(stderr, "%s:%d [%s]: assertion failed [%s]",  __FILE__, __LINE__, __PRETTY_FUNCTION__, #x); exit(EXIT_FAILURE); }}
//#define EXIT_IF_NULL(x) if(!(#x)) { fprintf(stderr, "%s:%d [%s]: returns NULL pointer",  __FILE__, __LINE__, __PRETTY_FUNCTION__); exit(EXIT_FAILURE); }

//#define SAFE_RELEASE(x) { if(x && *x){ safe_free((void**)x); *x = NULL; } }
//#define ERROR_EXIT() { perror(strerror(errno)); exit(errno); }

//===============================================================
#define STRING_INIT_BUFFER 16
#define ALLOC_ALIGN 16

#define HASHTABLE_SIZE 16
//===============================================================
// GLOBAS
//===============================================================
enum {
    ST_OK,
    ST_ERR,
    ST_NOT_FOUND,
    ST_EMPTY,
    ST_EXISTS,
    ST_OUT_OF_RANGE,
    ST_SIZE_EXCEED,
    ST_UNKNOWN
};

//======================================================================================================================
//
//======================================================================================================================

typedef struct ht_key
{
    union
    {
        char s[8];
        uint64_t i;

    };
} ht_key_t;

typedef struct ht_value
{
    void* ptr;
    size_t size;
} ht_value_t;

typedef struct _ht_item_t
{
    ht_key_t*   key;
    ht_value_t* value;
    uint64_t hash;
    struct _ht_item_t* next;
} ht_item_t;

typedef struct _hashtable_t
{
    ht_item_t* table[HASHTABLE_SIZE];
} hashtable_t;

static int ht_init(hashtable_t** ht);

static void ht_destroy(hashtable_t* ht);

static int ht_set(hashtable_t* ht, ht_key_t* key, ht_value_t* value);
static int ht_get(hashtable_t* ht, ht_key_t* key, ht_value_t** value);


typedef void(*ht_forach_cb)(uint64_t, ht_key_t*, ht_value_t*);

static int ht_foreach(hashtable_t* ht, ht_forach_cb cb);

static int ht_create_key_i(uint64_t keyval, ht_key_t** key)
{
    *key = malloc(sizeof(ht_key_t));
    (*key)->i = keyval;

    return ST_OK;
}

static int ht_create_value(void* p, size_t size, ht_value_t** value)
{
    *value = malloc(sizeof(ht_value_t));
    (*value)->ptr = p;
    (*value)->size = size;

    return ST_OK;
}


//======================================================================================================================
//======================================================================================================================


static hashtable_t* alloc_table = NULL;

struct _IO_FILE * stdout_orig;
struct _IO_FILE * stderr_orig;
struct _IO_FILE * stdtrace;
static FILE* stdtest;
//static char* stdtest_buf;
//static size_t stdtest_size;
static const size_t size_npos = (size_t)-1;
// init gloabls

static void string_init_globals();

static int init_gloabls()
{
    stdout_orig = stdout;
    stderr_orig = stderr;

    ht_init(&alloc_table);

    //stdtest = open_memstream(&stdtest_buf, &stdtest_size);
    stdtest = fopen("/dev/null", "w");
    stdtrace = fopen("/dev/null", "w");

    string_init_globals();

    return 0;
}


static void enable_stdout(bool b)
{
    if(b) stdout = stdout_orig;
    else stdout = stdtest;
}

static void enable_stderr(bool b)
{
    if(b) stderr = stderr_orig;
    else stderr = stdtest;
}


static int globals_shutdown()
{
    ht_destroy(alloc_table);
    fclose(stdtest);

    return ST_OK;
}


//======================================================================================================================
// blob operations
//======================================================================================================================

typedef struct
{
    uint64_t id;
    uint64_t ref;
    uint64_t size;
} object_t;

static void* object_create(size_t size)
{
    size_t csize = size+sizeof(object_t);
    char* ptr = malloc(csize);
    object_t b;
    b.id = (uint64_t)ptr;
    b.ref = 1;
    b.size = size;

    memcpy(ptr, &b, sizeof(object_t));


    // registration
    ht_key_t * key = NULL;
    ht_create_key_i((uint64_t)ptr, &key);

    ht_value_t* val = NULL;
    ht_create_value(ptr, csize, &val);

    ht_set(alloc_table, key, val);
    //-------------

    return ptr+sizeof(object_t);
}

static void* object_share(void* a, size_t obj_size)
{
    object_t* b = (object_t *) ((char*)a - obj_size);
    //atomic_fetch_add(&b->ref, 1);
    b->ref++;
    return a;
}

static void* object_copy(void* a, size_t obj_size)
{
    object_t* c = (object_t *) ((char*)a - obj_size);

    size_t csize = c->size+sizeof(object_t);
    uint64_t* ptr = malloc(csize);
    object_t b;
    b.id = (uint64_t)ptr;
    b.ref = 1;
    b.size = c->size;

    memcpy(ptr, &b, sizeof(object_t));

    // registration
    ht_key_t * key = NULL;
    ht_create_key_i((uint64_t)ptr, &key);

    ht_value_t* val = NULL;
    ht_create_value(ptr, csize, &val);

    ht_set(alloc_table, key, val);
    //-------------

    return ptr+sizeof(object_t);
}

static void object_release(void* a, size_t obj_size)
{
    object_t* c = (object_t *) ((char*)a - obj_size);

    if(c->ref > 0)
    {
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

typedef struct alloc_stat
{
    uint64_t size;
} alloc_stat_t;

static void _record_alloc_set(void* ptr, size_t size)
{
    alloc_stat_t* stat = malloc(sizeof(alloc_stat_t));
    stat->size = size;

    ht_key_t* key = NULL;
    ht_create_key_i((uint64_t)(uint64_t*)ptr, &key);

    ht_value_t* val = NULL;
    ht_create_value(stat, sizeof(struct alloc_stat), &val);


    ht_set(alloc_table, key, val);
}

static int _record_alloc_get(void* ptr, alloc_stat_t** stat)
{
    ht_value_t* val = NULL;
    ht_key_t* key = calloc(sizeof(ht_key_t), 1);
    key->i = PTR_TO_U64(ptr);
    int res = ht_get(alloc_table, key, &val);
    free(key);
    if(res != ST_OK)
        return res;


    *stat = malloc(val->size);
    memcpy(*stat, val->ptr, val->size);

    return ST_OK;
}

static void safe_free(void** pp)
{
    if(pp && *pp)
    {
        void* p = *pp;

        alloc_stat_t *stat = NULL;

        if (_record_alloc_get(p, &stat) == ST_OK) {

            _record_alloc_set(p, 0);

            if(stat->size > 0) {
                fprintf(stdtrace, "[safe_free] found address: 0x%08lx size: %lu\n", (uint64_t) (uint64_t *) p,
                        stat->size);

                free(p);
                *pp = NULL;
            }


        }

        free(stat);
    }
}


//// zeros allocated memory
static void* allocz(void* dst, size_t size)
{
    size_t asize = size + (size % ALLOC_ALIGN);
    char *v = realloc(dst, asize);

    if(dst) {
        alloc_stat_t *stat = NULL;

        if (_record_alloc_get(dst, &stat) == ST_OK) {
            fprintf(stdtrace, "[allocz] found hash: 0x%08lx old_size: %lu new_size: %lu\n", (uint64_t) (uint64_t *) dst,
                    stat->size, asize);

            if (asize > stat->size) {
                size_t zsize = asize - stat->size;
                char *oldp = v + stat->size;
                memset(oldp, 0, zsize);
            }

            free(stat);
        }
    }
    else
    {
        memset(v, 0, asize);
    }

    _record_alloc_set(v, asize);
    return v;
}


static void* alloc_strict(void* dst, size_t size)
{
    return realloc(dst, size);
}


static void* alloc_zstrict(void* dst, size_t size)
{
    size_t asize = size;
    char *v = realloc(dst, asize);

    if(dst) {
        alloc_stat_t *stat = NULL;

        if (_record_alloc_get(dst, &stat) == ST_OK) {
            fprintf(stdtrace, "[alloc_zstrict] found hash: 0x%08lx old_size: %lu new_size: %lu\n", (uint64_t) (uint64_t *) dst,
                    stat->size, asize);

            if (asize > stat->size) {
                size_t zsize = asize - stat->size;
                char *oldp = v + stat->size;
                memset(oldp, 0, zsize);
            }
        }
    }
    else
    {
        memset(v, 0, asize);
    }

    _record_alloc_set(v, asize);
    return v;

}

static void* mmove(void* dst, const void* src, size_t size)
{
    return memmove(dst, src, size);
}

static void* mcopy(void* dst, const void* src, size_t size)
{
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

static uint64_t crc64s(uint64_t crc, const unsigned char *s, uint64_t l) {
    uint64_t j;

    for (j = 0; j < l; j++) {
        uint8_t byte = s[j];
        crc = crc64_tab[(uint8_t)crc ^ byte] ^ (crc >> 8);
    }
    return crc;
}

static uint64_t crc64(uint64_t i)
{
    uint64_t l = 8UL;
    uint64_t crc = 0;
    uint64_t j;
    for (j = 0; j < l; j++) {
        uint64_t s = 0xFFUL << (j*8);
        uint8_t byte = (uint8_t)((i & s)>>(j*8));
        crc = crc64_tab[(uint8_t)crc ^ byte] ^ (crc >> 8);
    }
    return crc;
}

//=======================================================================
// HASH TABLE
//=======================================================================

static unsigned long ht_hash(const char* _str)
{
    const unsigned char* str = (const unsigned char*)_str;
    unsigned long hash = 5381;
    int c;

    while((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


static int ht_init(hashtable_t** ht)
{
    *ht = calloc(sizeof(hashtable_t), 1);
    return ST_OK;
}

static void ht_destroy_item(ht_item_t* item)
{
    free(item->key);
    free(item->value->ptr);
    free(item->value);
    free(item);
}

static void ht_destroy_items_line(ht_item_t* start_item)
{
    ht_item_t* next = start_item;
    ht_item_t* tmp = NULL;
    while(next)
    {
        tmp = next;
        next = next->next;

        ht_destroy_item(tmp);
    }
}


static void ht_destroy(hashtable_t* ht)
{
    for(size_t i = 0; i < HASHTABLE_SIZE; ++i) {
        ht_destroy_items_line(ht->table[i]);
    }

    free(ht);
}


static int ht_create_item(ht_item_t** pitem, uint64_t name_hash, ht_value_t* value)
{
    ht_item_t* item;
    if((item = calloc(sizeof(ht_item_t), 1)) == NULL)
    {
        fprintf(stderr, "[ht_create_item] can't alloc");
        return ST_ERR;
    }

    item->hash = name_hash;
    item->value = value;

    *pitem = item;

    return ST_OK;
}


static int ht_set(hashtable_t* ht, ht_key_t* key, ht_value_t* value)
{
    uint64_t hash = key->i;
    size_t bin =  hash % HASHTABLE_SIZE;

    ht_item_t* item = ht->table[bin];
    ht_item_t* prev = NULL;
    while(item)
    {
        if(item->hash == hash)
            break;

        prev = item;
        item = item->next;
    }


    if(item && item->hash == hash)
    {
        item->value = value;
        free(key);
    }
    else
    {
        ht_item_t* new_item = NULL;
        if((ht_create_item(&new_item, hash, value)) != ST_OK)
        {
            return ST_ERR;
        }

        new_item->key = key;

        if(prev)
            prev->next = new_item;
        else
            ht->table[bin] = new_item;
    }

    return ST_OK;
}
static int ht_get(hashtable_t* ht, ht_key_t* key, ht_value_t** value)
{
    uint64_t hash = key->i;
    uint64_t bin =  hash % HASHTABLE_SIZE;

    ht_item_t* item = ht->table[bin];

    while(item)
    {
        if(item->hash == hash)
        {
            *value = item->value;
            return ST_OK;
        }

        item = item->next;
    }

    return ST_NOT_FOUND;
}

static int ht_foreach(hashtable_t* ht, ht_forach_cb cb)
{
    for(size_t i = 0; i < HASHTABLE_SIZE; ++i) {
        ht_item_t* next = ht->table[i];
        while(next)
        {
            cb(next->hash, next->key, next->value);
            next = next->next;
        }
    }

    return ST_OK;
}

//=======================================================================
// DYNAMIC ALLOCATOR
//=======================================================================


typedef struct {
    char *ptr;
    size_t size;
    size_t used;
    size_t mul;

} dynamic_allocator;

static int da_init_n(dynamic_allocator** a, size_t size) {
    *a  = allocz(NULL, sizeof(dynamic_allocator));

    (*a)->ptr = allocz(NULL, size);


    (*a)->size = size;
    (*a)->mul = 1;

    return ST_OK;
}

static int da_init(dynamic_allocator** a) {

    return da_init_n(a, STRING_INIT_BUFFER);
}

static int da_release(dynamic_allocator** a)
{
    if(a && *a) {

        safe_free((void**)&(*a)->ptr);
        safe_free((void**)a);
    }

    return ST_OK;
}

static int da_reallocate(dynamic_allocator *a, size_t size) {
    a->size += size * a->mul++;
    a->ptr = allocz(a->ptr, a->size);

    return 0;
}

static int da_crop_tail(dynamic_allocator* a, size_t pos)
{
    //TODO make faster
    a->size = a->used-pos;
    char* newbuff = allocz(NULL, a->size);
    memcpy(newbuff, &a->ptr[pos], a->size);
    free(a->ptr);
    a->ptr = newbuff;
    a->used = a->size;
    return ST_OK;
}

static int da_shink(dynamic_allocator *a, bool strict) {
    if(strict)
        a->ptr = alloc_zstrict(a->ptr, a->used);
    else
        a->ptr = allocz(a->ptr, a->used);
    a->size = a->used;

    return 0;
}

static int da_check_size(dynamic_allocator *a, size_t new_size) {
    if (a->size < a->used + new_size)
        da_reallocate(a, new_size);

    return 0;
}

static int da_append(dynamic_allocator *a, const char *data, size_t size) {
    da_check_size(a, size);

    mcopy(a->ptr + a->used, data, size);
    a->used += size;

    return ST_OK;
}

static int da_sub(dynamic_allocator* a, size_t pos, dynamic_allocator** b)
{
    if(pos > a->used)
        return ST_OUT_OF_RANGE;

    size_t b_size = a->used - pos;
    da_init_n(b, b_size);
    da_append(*b, a->ptr+pos, b_size);

    a->used -= b_size;
    da_shink(a, true);

    return ST_OK;

}

static int da_dub(dynamic_allocator* a, dynamic_allocator** b)
{
    size_t b_size = a->used;
    da_init_n(b, b_size);
    da_append(*b, a->ptr, b_size);

    return ST_OK;
}

static int da_merge(dynamic_allocator *a, dynamic_allocator *b) {
    da_shink(a, true);
    da_shink(b, true);

    size_t nb_size = a->size + b->size;

    da_reallocate(a, nb_size);

    mcopy(a->ptr+a->used, b->ptr, b->size);

    a->used += b->size;

    da_release(&b);

    return ST_OK;
}

static int da_remove_seq(dynamic_allocator* a, size_t pos, size_t n)
{
    dynamic_allocator* b = NULL;

    da_sub(a, pos-n, &b);

    da_crop_tail(b, n-1);

    da_merge(a, b);

    return ST_OK;

}


//=======================================================================
// GENERIC VECTOR
//=======================================================================
typedef struct vector
{
    dynamic_allocator* alloc;
    size_t size;
    size_t elem_size;
} vector_t;

static int vector_init(vector_t** vec, size_t elem_size)
{
    *vec = allocz(NULL, sizeof(vector_t));
    da_init_n(&(*vec)->alloc, elem_size * 10);
    (*vec)->elem_size = elem_size;

    return ST_OK;
}

static int vector_release(vector_t* vec)
{
    da_release(&vec->alloc);
    free(vec);

    return ST_OK;
}

static size_t vector_size(vector_t* vec) { return vec->size; }

static int vector_add(vector_t* vec, const void* elem)
{
    da_append(vec->alloc, (const char*)elem, vec->elem_size);
    vec->size++;

    return ST_OK;
}


static int vector_get(vector_t* vec, size_t idx, void** elem)
{
    if(idx >= vec->size)
        return ST_OUT_OF_RANGE;

    *elem = (void*)&(vec->alloc->ptr[vec->elem_size*idx]);

    return ST_OK;
}

static int vector_set(vector_t* vec, size_t idx, void* elem)
{
    if(idx >= vec->size)
        return ST_OUT_OF_RANGE;

    void* el = (void*)&(vec->alloc->ptr[vec->elem_size*idx]);

    memcpy(el, elem, vec->elem_size);

    return ST_OK;

}

typedef void(*vector_foreach_cb)(size_t,size_t, void*, void*);

static void vector_foreach(vector_t* vec, void* ctx, vector_foreach_cb cb)
{
    size_t n = vec->size;
    for(size_t i = 0; i < n; ++i)
    {
        void* v = (void*)&(vec->alloc->ptr[vec->elem_size*i]);
        cb(i, vec->elem_size, ctx, v);
    }
}


//=======================================================================
// STRING
//=======================================================================

typedef struct {
    dynamic_allocator* alloc;
    size_t size;
    uint64_t nt;
} string;

typedef struct skey_value
{
    string* key;
    string* value;
} skey_value_t;

static string* string_null = NULL;

static int string_init(string** sp) {
    *sp = allocz(NULL, sizeof(string));
    da_init(&((*sp)->alloc));
    (*sp)->size = 0;
    (*sp)->nt = false;

    return ST_OK;
}

static int string_release(string** s)
{
    if(s && *s) {
        da_release(&(*s)->alloc);

        safe_free((void**)s);

    }

    return ST_OK;
}

static int string_dub(string* s, string** ns)
{
    *ns = allocz(NULL, sizeof(string));
    da_dub(s->alloc, &(*ns)->alloc);

    (*ns)->size = s->size;
    (*ns)->nt = s->nt;

    return ST_OK;
}

static int string_append(string *s, const char *str) {
    size_t len = strlen(str);
    s->size += len;
    s->nt = false;
    da_append(s->alloc, str, len);

    return ST_OK;

}

//append null-terminated string
static int string_appendz(string *s, const char *str) {
    size_t len = strlen(str) + 1;
    s->size += len;
    s->nt = true;
    da_append(s->alloc, str, len);

    return ST_OK;

}

static void string_init_globals()
{
    string_init(&string_null);
    string_appendz(string_null,"(null)");
}


static int string_appendn(string *s, const char *str, size_t len) {
    s->size += len;
    s->nt = false;
    da_append(s->alloc, str, len);

    return ST_OK;

}

static int string_appendnz(string *s, const char *str, size_t len) {
    s->size += len;
    s->nt = true;
    da_append(s->alloc, str, len);
    da_append(s->alloc, "\0", 1);

    return ST_OK;

}

static int string_create(string **s, const char *str) {
    string_init(s);

    string_append(*s, str);

    return ST_OK;
}

static int string_createz(string **s, const char *str) {
    string_init(s);

    string_appendz(*s, str);

    return ST_OK;
}

static char string_get_ch(string* s, size_t idx)
{
    if(idx > s->size)
        EXIT_ERROR("Index is out of bound");

    return s->alloc->ptr[idx];
}

static int string_add(string *a, string *b) {
    da_merge(a->alloc, b->alloc);
    a->size += b->size;
    a->nt = b->nt;
    return ST_OK;
}

static int string_pop_head(string* s)
{
    --s->alloc->used;
    --s->size;
    da_shink(s->alloc, true);

    if(s->nt)
        s->alloc->ptr[s->alloc->size-1] = '\0';

    return ST_OK;
}

static int string_crop_tail(string* s)
{
    da_crop_tail(s->alloc, 1);

    return ST_OK;
}

static size_t string_find_last_char(string* s, char ch)
{
    for(size_t i = s->size; i != 0; --i)
    {
        char cur = s->alloc->ptr[i];
        if(cur == ch)
            return i;
    }

    return size_npos;

}

static int string_starts_withz(string* s, const char* str)
{
    size_t str_len = strlen(str);
    if(str_len > s->size)
        return ST_SIZE_EXCEED;

    if(memcmp(s->alloc->ptr, str, str_len) == 0)
        return ST_OK;

    return ST_NOT_FOUND;
}

static int string_compare(string* a, string* b)
{
    if(a->size != b->size)
        return ST_NOT_FOUND;

    if(memcmp(a->alloc->ptr, b->alloc->ptr, a->size) == 0)
        return ST_OK;


    return ST_NOT_FOUND;
}

static int string_comparez(string* a, const char* cmp)
{
    if(strncmp(a->alloc->ptr, cmp, strlen(cmp)) == 0)
        return ST_OK;

    return ST_NOT_FOUND;
}

static void string_map_region(string* s, size_t beg, size_t end, char** sb, char** se)
{
    if((beg > 0 && beg <= s->size) && (end > 0 && end <= s->size))
        EXIT_ERROR("Indexes are out of bound");

    *sb = s->alloc->ptr + beg;
    *se = s->alloc->ptr + end;
}

static void string_map_string(string* s, char** sb, char** se)
{
    string_map_region(s, 0, s->size, sb, se);
}

static int string_to_u64(string* s, uint64_t* ul)
{

    uint64_t res = 0;

    for (size_t i = 0; i < s->size; ++i)
        res = res * 10 + string_get_ch(s, i) - '0';

    *ul = res;

    return ST_OK;

}

static void string_fprint(string *a, FILE* output) {
    //printf("%.*s", a->size, a->alloc.ptr);
    fwrite(a->alloc->ptr, 1, a->size, output);
    fwrite("\n", 1, 1, output);
    fflush(output);
}

//=======================================================================
// SLIST
//=======================================================================

typedef struct snode
{
    string* s;
    struct snode* prev;
    struct snode* next;

} snode;

typedef struct
{
    snode* head;
    snode* tail;
    size_t size;
    snode* cur;
    char delm;

} slist;


static void slist_init(slist** sl)
{
    *sl = allocz(NULL, sizeof(slist));
    (*sl)->delm = ',';
}

static void snode_init(snode** node)
{
    *node = allocz(NULL ,sizeof(snode));
}


static void slist_set_delm(slist* sl, char delm)
{
    sl->delm = delm;
}

static void snode_append(slist* sl, snode* node, snode* prev, string* s)
{
    if(!node)
        snode_init(&node);

    if(node->s == NULL)
    {
        node->s = s;
        node->prev = prev;

        if(prev) prev->next = node;

        sl->size++;
        sl->tail = node;
        return;
    }

    snode_append(sl, node->next, node, s);

}

static void slist_append(slist* sl, string* s)
{
    if(!sl->head)
        snode_init(&sl->head);

    if(!sl->tail)
        snode_init(&sl->tail);

    snode_append(sl, sl->head, NULL, s);

}

static string* slist_pop_head(slist* sl)
{
    if(sl->size == 0)
        EXIT_ERROR("Empty list");

    snode* tmp = sl->head;
    sl->head = tmp->prev;
    sl->size--;

    string* s = tmp->s;
    free(tmp);

    return s;
}

static void slist_init_current(slist* sl)
{
    if(sl->size == 0)
        EXIT_ERROR("Empty list");

    sl->cur = sl->head;
}

static string* slist_next(slist* sl)
{
    if(sl->cur == NULL)
        return NULL;

    string* s = sl->cur->s;

    sl->cur = sl->cur->next;

    return s;
}

static int slist_release(slist** sl, bool srelease)
{
    if(sl && *sl) {
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

static void slist_merge(slist* a, slist* b)
{
    if(a->size == 0 && b->size == 0)
        EXIT_ERROR("lists must contain at least one element");


    a->tail->next = b->head;
    b->head->prev = a->tail;
    a->size+= b->size;
    a->tail = b->tail;
}

static void slist_remove(slist* sl, string* s)
{
    snode* head = sl->head;

    while(head)
    {
        if(head->s == s)
        {
            snode* hn = head->next;
            snode* hp = head->prev;

            hn->prev = hp;
            hp->next = hn;

            free(head);

            return;

        }

        head = head->next;
    }
}

static int slist_find_eq(slist* sl, string* s, string** found)
{
    snode* head = sl->head;

    while(head)
    {
        if(string_compare(head->s, s) == ST_OK) {
            *found = head->s;
            return ST_OK;
        }

        head = head->next;
    }

    return ST_NOT_FOUND;
}

static void slist_fprint(slist* sl, FILE* output)
{
    snode* head = sl->head;
    while(head)
    {
        string_fprint(head->s, output);
        head = head->next;
    }
}

static void slist_rfprint(slist* sl, FILE* output)
{
    snode* tail = sl->tail;

    while(tail)
    {

        string_fprint(tail->s, output);
        tail = tail->prev;
    }
}

static int string_remove_seq(string* s, size_t pos, size_t n)
{
    da_remove_seq(s->alloc, pos, n);
    s->size = s->alloc->used;

    return ST_OK;
}

static int string_remove_dubseq(string* s, char delm)
{
    size_t j = 0;
    while(j < s->size) {
        char *cur = s->alloc->ptr;
        size_t i = 0;
        size_t n = 0;

        while (cur[j+(i++)] == delm) ++n;



        if(n > 1) {
            string_remove_seq(s, j+i, n);
            j = 0; // skip due to internal buffer changed
        }

        ++j;

        if( j >= s->size)
            break;

    }

    return ST_OK;
}

static int string_split(string* s, char delm, slist** sl)
{
    if(s->size == 0)
        return ST_EMPTY;


    string_remove_dubseq(s, delm);

    slist_init(sl);
    char* cb = NULL;
    char* end = NULL;


    string_map_string(s, &cb, &end);
    char* ccur = cb;

    while(ccur  <= end)
    {
        if(*ccur == delm || *ccur == '\0')
        {

            string* ss = NULL;
            string_init(&ss);
            string_appendnz(ss, cb, (size_t)(ccur-cb));

            slist_append(*sl, ss);

            cb = ++ccur;
        }

        ++ccur;
    }

    return ST_OK;
}

static const char strip_dict[] = { '\0', '\n', '\r', '\t', ' ', '"', '\"', '\'' };

static bool check_strip_dict(char ch)
{
    for(size_t i = 0; i < sizeof(strip_dict)/sizeof(char); ++i)
        if(ch == strip_dict[i])
            return true;

    return false;
}

static int string_rstrip(string* s)
{
    while(s->size > 0)
    {
        size_t idx = s->size - 1;
        char sch = string_get_ch(s, idx);

        if(check_strip_dict(sch))
            string_pop_head(s);
        else
            return ST_OK;

    }

    return ST_OK;
}

static int string_lstrip(string* s)
{
    for(size_t j = 0; j < s->size; ++j) {

        char sch = string_get_ch(s, j);
        if(check_strip_dict(sch))
            string_crop_tail(s);
        else
            return ST_OK;
    }
    return ST_OK;
}

static int string_strip(string* s) {

    if(s->size < 3)
        return ST_SIZE_EXCEED;

    string_rstrip(s);
    string_lstrip(s);

    return ST_OK;
}


static int test_slist()
{
    slist* sl = NULL;
    slist_init(&sl);

    string* s1 = NULL;
    string* s2 = NULL;
    string* s3 = NULL;
    string* s4 = NULL;
    string* s5 = NULL;
    string* s6 = NULL;
    string* s7 = NULL;
    string* s55 =NULL;


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


    string* found = NULL;
    slist_find_eq(sl, s55, &found);


    slist_remove(sl, found);

    slist_fprint(sl, stdtest);
    fprintf(stdtest, "\n\n");
    slist_rfprint(sl, stdtest);
    fprintf(stdtest, "\n\n");


    slist* sl2 = NULL;
    slist_init(&sl2);

    string* s21 = NULL;
    string* s22 = NULL;
    string* s23 = NULL;
    string* s24 = NULL;
    string* s25 = NULL;
    string* s26 = NULL;
    string* s27 = NULL;


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
    fprintf(stdtest,"\n\n");


    slist_merge(sl, sl2);

    slist_fprint(sl, stdtest);
    fprintf(stdtest, "\n\n");
    slist_rfprint(sl, stdtest);
    fprintf(stdtest, "\n\n");


    string* text = NULL;
    string_init(&text);


    string_append(text, "SIZE=\"240057409536\" MODEL=\"TOSHIBA-TR150   \" LABEL=\"\" UUID=\"\" MOUNTPOINT=\"\"\n");
    string_append(text, "NAME=\"sda1\" FSTYPE=\"vfat\" SCHED=\"cfq\" SIZE=\"536870912\" MODEL=\"\" LABEL=\"\" UUID=\"B58E-8A00\" MOUNTPOINT=\"/boot\"\n");
    string_append(text, "NAME=\"sda2\" FSTYPE=\"swap\" SCHED=\"cfq\" SIZE=\"17179869184\" MODEL=\"\" LABEL=\"\" UUID=\"c8ae3239-f359-4bff-8994-c78d20efd308\" MOUNTPOINT=\"[SWAP]\"\n");
    string_append(text, "NAME=\"sda3\" FSTYPE=\"ext4\" SCHED=\"cfq\" SIZE=\"42949672960\" MODEL=\"\" LABEL=\"\" UUID=\"ecff6ff7-1380-44df-a1a5-e2a4e10eba4e\" MOUNTPOINT=\"/\"\n");
    string_append(text, "NAME=\"sda4\" FSTYPE=\"ext4\" SCHED=\"cfq\" SIZE=\"179389931008\" MODEL=\"\" LABEL=\"\" UUID=\"e77e913c-9829-4750-b3ee-ccf4e641d67a\" MOUNTPOINT=\"/home\"\n");
    string_append(text, "NAME=\"sdb\" FSTYPE=\"\" SCHED=\"cfq\" SIZE=\"2000398934016\" MODEL=\"ST2000DM001-1CH1\" LABEL=\"\" UUID=\"\" MOUNTPOINT=\"\"\n");
    string_append(text, "NAME=\"sdb1\" FSTYPE=\"ext4\" SCHED=\"cfq\" SIZE=\"2000397868544\" MODEL=\"\" LABEL=\"\" UUID=\"cdc9e724-a78b-4a25-9647-ad6390e235c3\" MOUNTPOINT=\"\"\n");


    slist* blklist = NULL;
    string_split(text, '\n', &blklist);

    fprintf(stdout, "------- LINES OF TOKENS -----------\n");
    slist_fprint(blklist, stdout);
    fflush(stdout);

    string* tk = NULL;
    slist_init_current(blklist);
    while((tk = slist_next(blklist)) != NULL) {

        fprintf(stdout, "------- TOKEN LINE-----------\n");
        string_fprint(tk, stdout);

        fprintf(stdout, "------- TOKEN SPLIT-----------\n");

        slist* tokens = NULL;
        string_split(tk,' ',&tokens);

        slist_fprint(tokens, stdout);

        slist_init_current(tokens);

        string *s = NULL;
        while ((s = slist_next(tokens)) != NULL) {
            slist *kv = NULL;
            string_split(s, '=', &kv);

            fprintf(stdout, "------- TOKEN KEY-VALUE -----------\n");
            slist_fprint(kv, stdout);


            slist_init_current(kv);
            string* key = slist_next(kv);
            string *val = slist_next(kv);

            string_strip(val);

            fprintf(stdout, "------- STRIPED TOKEN KEY-VALUE -----------\n");
            string_fprint(key, stdout);
            string_fprint(string_null, stdout);

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


static size_t get_sfile_size(const char* filename) {
    struct stat st;
    stat(filename, &st);
    return (size_t) st.st_size;
}

typedef void(*cmd_exec_cb)(void *, slist*);

static void sfile_mmap(const char* filename, string* s)
{
    size_t filesize = get_sfile_size(filename);
    //Open file
    int fd = open(filename, O_RDONLY, 0);

    //Execute mmap
    void* data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);

    //Write the mmapped data
    string_init(&s);
    string_appendn(s, (char*)data, filesize);

    //Cleanup
    munmap(data, filesize);
    close(fd);
}


static size_t get_fd_file_size(int fd) {
    struct stat st;
    fstat(fd, &st);
    return (size_t) st.st_size;
}

static void fd_file_mmap(int fd, string* s)
{
    size_t filesize = get_fd_file_size(fd);

    //Execute mmap
    void* data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);


    //Write the mmapped data
    string_init(&s);
    string_appendn(s, (char*)data, filesize);

    //Cleanup
    munmap(data, filesize);
    close(fd);
}

static int cmd_execute(const char *cmd, void *ctx, cmd_exec_cb cb) {
    FILE *fpipe;

    if (!(fpipe = popen(cmd, "r")))
        return ST_ERR;

    char line[1024] = {0};


    slist* sl;
    slist_init(&sl);

    while (fgets(line, sizeof(line), fpipe)) {
        string* s = NULL;

        string_createz(&s, line);
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


typedef struct df_stat
{
    string* dev;

    uint64_t total;
    uint64_t used;
    uint64_t avail;
    uint64_t use;


} df_stat_t;

typedef struct {
    hashtable_t* stats;
    uint64_t skip_first;
} df_t;


static void df_init(df_t** df) {
    *df = allocz(NULL, sizeof(df_t));
    (*df)->stats = allocz(NULL, sizeof(hashtable_t));
    (*df)->skip_first = 1;
}

static int ht_set_df(hashtable_t* ht, string* key, df_stat_t* stat)
{

    ht_key_t* hkey = NULL;
    uint64_t hash = crc64s(0, (uint8_t*)key->alloc->ptr, key->size);
    ht_create_key_i(hash, &hkey);

    ht_value_t* val = NULL;
    ht_create_value(stat, sizeof(df_stat_t), &val);

    ht_set(ht, hkey, val);

    return ST_OK;
}

static void df_callback(void *ctx, slist* lines)
{
    df_t* df = (df_t*)ctx;

    fprintf(stdout, "------- LINES OF TOKENS -----------\n");
    slist_fprint(lines, stdout);
    fflush(stdout);

    string* tk = NULL;
    slist_init_current(lines);
    while((tk = slist_next(lines)) != NULL) {

        if(string_starts_withz(tk, "/dev/") != ST_OK)
            continue;

        fprintf(stdout, "------- TOKEN LINE-----------\n");
        string_fprint(tk, stdout);

        fprintf(stdout, "------- TOKEN SPLIT-----------\n");

        slist* tokens = NULL;
        string_split(tk,' ',&tokens);

        slist_fprint(tokens, stdout);

        slist_init_current(tokens);

        //==============================
        // init df_stat

        df_stat_t* stat = allocz(NULL, sizeof(df_stat_t));


        //==============================


        string *s = NULL;
        string* sname = NULL;
        uint64_t k = 0;
        while ((s = slist_next(tokens)) != NULL) {

            string_strip(s);

            switch(k) {
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
                    string_pop_head(s);
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

static void df_execute(df_t* dfs) {

    string* s;
    string_createz(&s, "df --block-size=1");

    char *cmd = NULL;
    char* cmd_end;
    string_map_string(s, &cmd, &cmd_end);
    cmd_execute(cmd, dfs, &df_callback);
}


enum
{
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
    hashtable_t* blk;
} sblkid_t;

typedef struct
{
    char* ns;
    char* ne;
    char* vs;
    char* ve;
} kv_pair;

enum
{
    SM_CHAR = 0,
    SM_EQUAL,
    SM_QUOTE,
    SM_SPACE
};


static int sm_token(char ch)
{
    if(ch == '"') return SM_QUOTE;
    if(ch == '=') return SM_EQUAL;
    if(isspace(ch)) return SM_SPACE;
    return SM_CHAR;
}

#define FSM_CALLBACK(x) ((size_t)&x)


typedef struct system
{
    hashtable_t* blk;
} system_t;


static int system_init(system_t** sys)
{
    *sys = allocz(NULL, sizeof(system_t));
    ht_init(&(*sys)->blk);

    return ST_OK;
}

static int vector_add_kv(vector_t* vec, string* key, string* val)
{
    skey_value_t* kv = allocz(NULL, sizeof(skey_value_t));
    string_dub(key, &kv->key);
    string_dub(val, &kv->value);

    vector_add(vec, kv);

    return ST_OK;
}

static int ht_set_s(hashtable_t* ht, string* key, vector_t* vec)
{

    ht_key_t* hkey = NULL;
    uint64_t hash = crc64s(0, (uint8_t*)key->alloc->ptr, key->size);
    ht_create_key_i(hash, &hkey);

    ht_value_t* val = NULL;
    ht_create_value(vec, sizeof(vector_t), &val);

    ht_set(ht, hkey, val);

    return ST_OK;
}




static void sblk_callback(void *ctx, slist* lines) {

    sblkid_t* sys = (sblkid_t*)ctx;
    ht_init(&sys->blk);

    fprintf(stdout, "------- LINES OF TOKENS -----------\n");
    slist_fprint(lines, stdout);
    fflush(stdout);

    string* tk = NULL;
    slist_init_current(lines);
    while((tk = slist_next(lines)) != NULL) {

        fprintf(stdout, "------- TOKEN LINE-----------\n");
        string_fprint(tk, stdout);

        fprintf(stdout, "------- TOKEN SPLIT-----------\n");

        slist* tokens = NULL;
        string_split(tk,' ',&tokens);

        slist_fprint(tokens, stdout);

        slist_init_current(tokens);

        //==============================
        // vector pair init

        vector_t* vec = NULL;
        vector_init(&vec, sizeof(skey_value_t));

        //==============================


        string *s = NULL;
        string* sname = NULL;
        while ((s = slist_next(tokens)) != NULL) {

            slist *kv = NULL;
            string_split(s, '=', &kv);

            fprintf(stdout, "------- TOKEN KEY-VALUE -----------\n");
            slist_fprint(kv, stdout);


            slist_init_current(kv);
            string* key = slist_next(kv);
            string *val = slist_next(kv);

            string_strip(val);

            if(string_comparez(key,"NAME") == ST_OK)
                string_dub(val, &sname);

            fprintf(stdout, "------- STRIPED TOKEN KEY-VALUE -----------\n");

            //add kv
            vector_add_kv(vec, key, val);

            string_fprint(key, stdout);
            string_fprint(val, stdout);

            slist_release(&kv, true);
        }


        //add to ht, not need to free
        ht_set_s(sys->blk, sname, vec);

        string_release(&sname);

        slist_release(&tokens, true);

    }

}

static int sblk_execute(sblkid_t *sblk) {
    static const char *options[] = {"NAME", "FSTYPE", "SCHED", "SIZE", "MODEL", "LABEL", "UUID", "MOUNTPOINT"};

    string* cmd = NULL;
    string_create(&cmd, "lsblk -i -P -b -o ");
    string_append(cmd, options[0]);


    size_t opt_size = sizeof(options) / sizeof(char *);
    for (size_t i = 1; i < opt_size; ++i) {
        string_append(cmd, ",");
        string_append(cmd, options[i]);
    }

    string_appendz(cmd, "");


    char *ccmd = NULL;
    char* cmd_end = NULL;
    string_map_string(cmd, &ccmd, &cmd_end);
    cmd_execute(ccmd, sblk, &sblk_callback);

    return ST_OK;
}


typedef struct device {
    string* name;
    //struct statvfs stats;
    //std::vector <uint64_t> stat;
    string* perf_read;
    string* perf_write;
    string* label;
    uint64_t size;
    uint64_t used;
    uint64_t avail;
    double perc;
    string* fsize;
    string* fuse;
    string* fs;
    string* mount;
    string* sysfolder;
    string* model;
    uint64_t child;
    vector_t childs;
} device_t;

//static int read_all_file(const char* path) {
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
//static int statfs_dev() {
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

struct A
{
    OBJECT_DECLARE()

    int i;
    char ch;

    char* str;

};

static uint64_t total_leak = 0;

void scan_alloc(uint64_t hash, ht_key_t* key, ht_value_t* val)
{
    fprintf(stderr,"[scan_alloc]: [0x%08lx] [0x%08lx] %lu bytes\n", hash, (uint64_t)val->ptr, val->size);
    total_leak += val->size;
    key->i = hash;
}


void scan_blk(uint64_t hash, ht_key_t* key, ht_value_t* val)
{
    vector_t* vec = (vector_t*)val->ptr;

    size_t sz = vector_size(vec);

    for(size_t i =0; i < sz; ++i)
    {
        skey_value_t* kv = NULL;
        vector_get(vec, i, (void**)&kv);
        fprintf(stderr,"[scan_blk][0x%08lx] %s=%s\n", hash, kv->key->alloc->ptr, kv->value->alloc->ptr);
    }


    key->i = hash;
}



void scan_df(uint64_t hash, ht_key_t* key, ht_value_t* val)
{
    df_stat_t* stat = (df_stat_t*)val->ptr;


    fprintf(stderr,"[scan_df][0x%08lx][%s] SIZE=%lu USED=%lu AVAIL=%lu USE=%lu%% \n", hash, stat->dev->alloc->ptr,
                                                    stat->total, stat->used, stat->avail, stat->use);



    key->i = hash;
}

int main() {

    init_gloabls();

    enable_stdout(true);
    enable_stderr(true);

    //test_slist();

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




    df_t* df;
    df_init(&df);
    df_execute(df);
    ht_foreach(df->stats, &scan_df);



    sblkid_t blk;
    sblk_execute(&blk);

    ht_foreach(blk.blk, &scan_blk);


    //ht_foreach(alloc_table, &scan_alloc);

    //fprintf(stderr, "Total leaked: %lu bytes\n", total_leak);

    globals_shutdown();

    return 0;
}

