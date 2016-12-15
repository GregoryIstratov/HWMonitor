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

struct ht_item_t
{
    char* name;
    char* value;
    uint64_t hash;
    struct ht_item_t* next;
};

struct hashtable_t
{
    struct ht_item_t* table[HASHTABLE_SIZE];
};

typedef struct
{
    uint64_t size;
    uint64_t hash;

} alloc_stat;

static struct hashtable_t* alloc_table = NULL;

static FILE* stdtest;
//static char* stdtest_buf;
//static size_t stdtest_size;
static const size_t size_npos = (size_t)-1;
// init gloabls

static void string_init_globals();

static uint64_t crc64(uint64_t i);

static int ht_init(struct hashtable_t** ht);

static int ht_set(struct hashtable_t* ht, uint64_t i, char* value);

static int ht_get(struct hashtable_t* ht, uint64_t i, char** value);

static void ht_destroy(struct hashtable_t* ht);

static int init_gloabls()
{
    ht_init(&alloc_table);
    //stdtest = open_memstream(&stdtest_buf, &stdtest_size);
    stdtest = fopen("/dev/null", "w");
    stderr = stdtest;
    stdout = stdtest;

    string_init_globals();

    return 0;
}

//===============================================================
// ALLOCATORS
//===============================================================

static void _record_alloc_set(void* ptr, size_t size)
{
    alloc_stat* stat = malloc(sizeof(alloc_stat));
    stat->size = size;
    ht_set(alloc_table, (uint64_t)(uint64_t*)ptr, (char*)stat);
}

static int _record_alloc_get(void* ptr, alloc_stat** stat)
{
    return ht_get(alloc_table, (uint64_t)(uint64_t*)ptr, (char**)stat);
}

static void safe_free(void** pp)
{
    if(pp && *pp)
    {
        void* p = *pp;

        alloc_stat *stat = NULL;

        if (_record_alloc_get(p, &stat) == ST_OK) {

            _record_alloc_set(p, 0);

            if(stat->size > 0) {
                fprintf(stderr, "[safe_free] found address: 0x%08lx size: %lu\n", (uint64_t) (uint64_t *) p,
                        stat->size);

                free(p);
                *pp = NULL;
            }
        }
    }
}

static void* alloc(void* dst, size_t size)
{
    size_t asize = size + (size % ALLOC_ALIGN);
    return realloc(dst, asize);
}

//// zeros allocated memory
//static void* allocz(void* dst, size_t size)
//{
//    size_t asize = size + (size % ALLOC_ALIGN);
//    char *v = realloc(dst, asize);
//
//    if(dst) {
//        alloc_stat *stat = NULL;
//
//        if (_record_alloc_get(dst, &stat) == ST_OK) {
//            fprintf(stderr, "[allocz] found hash: 0x%08lx old_size: %lu new_size: %lu\n", (uint64_t) (uint64_t *) dst,
//                    stat->size, asize);
//
//            if (asize > stat->size) {
//                size_t zsize = asize - stat->size;
//                char *oldp = v + stat->size;
//                memset(oldp, 0, zsize);
//            }
//        }
//    }
//    else
//    {
//        memset(v, 0, asize);
//    }
//
//    _record_alloc_set(v, asize);
//    return v;
//}

// zeros allocated memory
static void* allocz(void* dst, size_t size)
{
    size_t asize = size + (size % ALLOC_ALIGN);
    char *v = malloc(asize);
    memset(v, 0, asize);

    if(dst) {
        alloc_stat *stat = NULL;

        if (_record_alloc_get(dst, &stat) == ST_OK) {
            fprintf(stderr, "[allocz] found hash: 0x%08lx old_size: %lu new_size: %lu\n", (uint64_t) (uint64_t *) dst,
                    stat->size, asize);


            memcpy(v, dst, stat->size);
            safe_free(&dst);
        }
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
        alloc_stat *stat = NULL;

        if (_record_alloc_get(dst, &stat) == ST_OK) {
            fprintf(stderr, "[alloc_zstrict] found hash: 0x%08lx old_size: %lu new_size: %lu\n", (uint64_t) (uint64_t *) dst,
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

//===============================================================

// GLOBALS



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


static int ht_init(struct hashtable_t** ht)
{
    *ht = calloc(sizeof(struct hashtable_t), 1);
    return ST_OK;
}

static void ht_destroy_item(struct ht_item_t* item)
{
    //free(item->name);
    free(item->value);
    free(item);
}

static void ht_destroy_items_line(struct ht_item_t* start_item)
{
    struct ht_item_t* next = start_item;
    struct ht_item_t* tmp = NULL;
    while(next)
    {
        tmp = next;
        next = next->next;

        ht_destroy_item(tmp);
    }
}


static void ht_destroy(struct hashtable_t* ht)
{
    for(size_t i = 0; i < HASHTABLE_SIZE; ++i) {
        ht_destroy_items_line(ht->table[i]);
    }
}

//static int ht_create_item_s(struct ht_item_t** pitem, const char* name, unsigned long name_hash, const char* value)
//{
//    struct ht_item_t* item;
//    if((item = malloc(sizeof(struct ht_item_t))) == NULL)
//    {
//        fprintf(stderr, "[ht_create_item] can't alloc");
//        return ST_ERR;
//    }
//
//    if((item->name = malloc(strlen(name))) == NULL)
//    {
//        free(item);
//        fprintf(stderr, "[ht_create_item] can't alloc");
//        return ST_ERR;
//    }
//
//    strcpy(item->name, name);
//
//    if((item->value = malloc(strlen(value))) == NULL)
//    {
//        free(item->name);
//        free(item);
//        fprintf(stderr, "[ht_create_item] can't alloc");
//        return ST_ERR;
//    }
//
//    strcpy(item->value, value);
//
//    item->hash = name_hash;
//
//    *pitem = item;
//
//    return ST_OK;
//}

static int ht_create_item(struct ht_item_t** pitem, uint64_t name_hash, char* value)
{
    struct ht_item_t* item;
    if((item = calloc(sizeof(struct ht_item_t), 1)) == NULL)
    {
        fprintf(stderr, "[ht_create_item] can't alloc");
        return ST_ERR;
    }

    item->value = value;

    item->hash = name_hash;

    *pitem = item;

    return ST_OK;
}

//static int ht_set_s(struct hashtable_t* ht, const char* name, const char* value)
//{
//    unsigned long hash = ht_hash(name);
//    unsigned long bin =  hash % HASHTABLE_SIZE;
//
//    struct ht_item_t* item = ht->table[bin];
//    struct ht_item_t* prev = NULL;
//    while(item)
//    {
//        if(item->hash == hash)
//            break;
//
//        prev = item;
//        item = item->next;
//    }
//
//
//    if(item && item->hash == hash)
//    {
//        char* tmp_val = NULL;
//        if((tmp_val = allocz(NULL,strlen(value))) == NULL)
//        {
//            fprintf(stderr, "[ht_set] can't alloc");
//            return ST_ERR;
//        }
//
//        free(item->value);
//        item->value = tmp_val;
//
//        strcpy(item->value, value);
//    }
//    else
//    {
//        struct ht_item_t* new_item = NULL;
//        if((ht_create_item_s(&new_item, name, hash, value)) != ST_OK)
//        {
//            return ST_ERR;
//        }
//
//        if(prev)
//            prev->next = new_item;
//        else
//            ht->table[bin] = new_item;
//    }
//
//    return ST_OK;
//}

static int ht_set(struct hashtable_t* ht, uint64_t i, char* value)
{
    uint64_t hash = crc64(i);
    size_t bin =  hash % HASHTABLE_SIZE;

    struct ht_item_t* item = ht->table[bin];
    struct ht_item_t* prev = NULL;
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
    }
    else
    {
        struct ht_item_t* new_item = NULL;
        if((ht_create_item(&new_item, hash, value)) != ST_OK)
        {
            return ST_ERR;
        }

        if(prev)
            prev->next = new_item;
        else
            ht->table[bin] = new_item;
    }

    return ST_OK;
}

//static int ht_get_s(struct hashtable_t* ht, const char* name, char** value)
//{
//    unsigned long hash = ht_hash(name);
//    unsigned long bin =  hash % HASHTABLE_SIZE;
//
//    struct ht_item_t* item = ht->table[bin];
//
//    while(item)
//    {
//        if(item->hash == hash)
//        {
//            *value = item->value;
//            return ST_OK;
//        }
//
//        item = item->next;
//    }
//
//    return ST_NOT_FOUND;
//}

static int ht_get(struct hashtable_t* ht, uint64_t i, char** value)
{
    uint64_t hash = crc64(i);
    uint64_t bin =  hash % HASHTABLE_SIZE;

    struct ht_item_t* item = ht->table[bin];

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

    mcopy(a->ptr+ a->used, data, size);
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

    da_crop_tail(b, n);

    da_merge(a, b);

    return ST_OK;

}


//=======================================================================
// STRING
//=======================================================================

typedef struct {
    dynamic_allocator* alloc;
    size_t size;
    uint64_t nt;
} string;

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
    s->size += len + 1;
    s->nt = true;
    da_append(s->alloc, str, len + 1);
    da_append(s->alloc, "\0", 1);

    return ST_OK;

}

static int string_create(string **s, const char *str) {
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

static int string_compare(string* a, string* b)
{
    if(a->size != b->size)
        return ST_NOT_FOUND;

    if(memcmp(a->alloc->ptr, b->alloc->ptr, a->size) == 0)
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
            string_remove_seq(s, j+i-1, n);
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
            string_appendnz(ss, cb, (size_t)(ccur-cb-1));

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
        }

    }


    {
        slist_release(&sl, true);
        slist_release(&sl2, true);
        slist_release(&blklist, true);
        string_release(&text);
    }

    return ST_OK;

}

//======================================================================

//uint64_t str2ull(const char *buf, size_t len)
//{
//    static const unsigned char ascii_hex[10] ={
//            0x30, // 0
//            0x31, // 1
//            0x32, // 2
//            0x33, // 3
//            0x34, // 4
//            0x35, // 5
//            0x36, // 6
//            0x37, // 7
//            0x38, // 8
//            0x39  // 9
//    };
//
//    // dec: 2016 - (0x32 0x30 0x32 0x36)
//    //   0   8  16   24  32   40   48   56   64  -  MSB 64
//    //---------------------------------------------------
//
//    uint64_t msb = 0;
//    msb |= 0x36;
//    msb |= 0x32<<8;
//    msb |= 0x30<<16;
//    msb |= 0x32<<24;
//
//    //  0   8  16  24  32- MSB 32
//    //---------------------------------------------------
//
//    uint32_t msb32 = 0;
//    msb32 |= 0x36;
//    msb32 |= 0x32<<8;
//    msb32 |= 0x30<<16;
//    msb32 |= 0x32<<24;
//
//
//    //---------------------------------------------------
//    //   64   56   48   40   32   24   16   8  0 - LSB 64
//    //---------------------------------------------------
//
//    uint64_t lsb = 0;
//    lsb |= 0x36<<56;
//    lsb |= 0x32<<48;
//    lsb |= 0x30<<32;
//    lsb |= 0x32<<24;
//
//
//    //---------------------------------------------------
//    //  32   24   16   8   0  - LSB 32
//    //---------------------------------------------------
//
//    uint32_t lsb32 = 0;
//    lsb32 |= 0x36<<24;
//    lsb32 |= 0x32<<16;
//    lsb32 |= 0x30<<8;
//    lsb32 |= 0x32;
//
//    uint64_t u = 0;
//    for(size_t i = 0, j = 0; i < len; ++i, j+=2)
//    {
//        uint64_t ch = (uint64_t)ascii_hex[buf[i]];
//        uint64_t byte = ch<<(j);
//        u |= byte;
//    }
//
//    return u;
//}


//static uint64_t str2ull(const char *str, size_t size) {
//    uint64_t res = 0;
//
//    for (size_t i = 0; i < size; ++i)
//        res = res * 10 + str[i] - '0';
//
//    return res;
//}

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


//static size_t get_sfile_size(const char* filename) {
//    struct stat st;
//    stat(filename, &st);
//    return (size_t) st.st_size;
//}
//
//typedef void(*cmd_exec_cb)(slist*, void *);
//
//static void sfile_mmap(const char* filename, string* s)
//{
//    size_t filesize = get_sfile_size(filename);
//    //Open file
//    int fd = open(filename, O_RDONLY, 0);
//
//    //Execute mmap
//    void* data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
//
//    //Write the mmapped data
//    string_init_n(s, filesize);
//    string_appendn(s, (char*)data, filesize);
//
//    //Cleanup
//    munmap(data, filesize);
//    close(fd);
//}
//
//
//static size_t get_fd_file_size(int fd) {
//    struct stat st;
//    fstat(fd, &st);
//    return (size_t) st.st_size;
//}
//
//static void fd_file_mmap(int fd, string* s)
//{
//    size_t filesize = get_fd_file_size(fd);
//
//    //Execute mmap
//    void* data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
//
//
//    //Write the mmapped data
//    string_init_n(s, filesize);
//    string_appendn(s, (char*)data, filesize);
//
//    //Cleanup
//    munmap(data, filesize);
//    close(fd);
//}
//
//static int cmd_execute(const char *cmd, void *ctx, cmd_exec_cb cb) {
//    FILE *fpipe;
//
//    if (!(fpipe = popen(cmd, "r"))) ERROR_EXIT();
//
////    char line[1024] = {0};
//
//
////    string output;
////    while (fgets(buff, sizeof(buff), fpipe)) {
////        string s
////        cb(line, ctx);
////    }
////
////    cb(&output, ctx);
////
//    pclose(fpipe);
//
//    return ST_OK;
//}
//
//
//enum {
//    DFS_TOTAL = 0,
//    DFS_USED = 1,
//    DFS_AVAIL = 2,
//    DFS_USE = 3
//
//};
//
//typedef struct {
//    uint64_t stat[4];
//    int skip_first;
//} DfSingle;
//
//
//static void dfs_init(DfSingle *dfs) {
//    memset(dfs, 0, sizeof(DfSingle));
//    dfs->skip_first = 1;
//}
//
//static void dfs_callback(slist* lines, void *ctx) {
//    DfSingle *dfs = (DfSingle *) ctx;
//
//    if (dfs->skip_first) {
//        dfs->skip_first = 0;
//        return;
//    }
//
//    regex_t re;
//    int reti = regcomp(&re, "([/a-zA-Z1-9]*)\\s*([0-9]*)\\s*([0-9]*)\\s*([0-9]*)\\s*([0-9]*)%\\s*([/a-zA-Z1-9-]*)",
//                       REG_EXTENDED);
//    if (reti) {
//        fprintf(stderr, "Could not compile regex\n");
//        exit(EXIT_FAILURE);
//    }
//
//    //printf(&source[0]);
//
////    static const size_t MAX_MATHCHES = 7;
////    static const size_t MAX_GROUPS = 7;
////
////    size_t nmatch = MAX_MATHCHES;
////    regmatch_t pmatch[MAX_GROUPS];
////    int rc;
////
////    if (0 != (rc = regexec(&re, source, nmatch, pmatch, 0))) {
////        printf("Failed to match '%s',returning %d.\n", source, rc);
////        exit(EXIT_FAILURE);
////    } else {
////
////        size_t j = 0;
////        //skip dev name and mount point
////        for (size_t i = 2; i < MAX_GROUPS - 1; ++i, ++j) {
////
////            size_t msize = (size_t) (pmatch[i].rm_eo - pmatch[i].rm_so);
////            const char *vals = &source[pmatch[i].rm_so];
////
////            dfs->stat[j] = str2ull(vals, msize);
////
//////
//////            printf("With the whole expression, "
//////                           "a matched substring \"%.*s\" is found at position %d to %d.\n",
//////                   pmatch[i].rm_eo - pmatch[i].rm_so, &source[pmatch[i].rm_so],
//////                   pmatch[i].rm_so, pmatch[i].rm_eo - 1);
////
////        }
////    }
////
//
//    regfree(&re);
//
//}
//
//static void dfs_execute(DfSingle *dfs, const char *dev) {
//
//    string s;
//    string_create(&s, "df --block-size=1");
//    string_append(&s, " ");
//    string_appendz(&s, dev);
//
//    char *cmd = NULL;
//    size_t cmd_len;
//    string_get(&s, &cmd, &cmd_len);
//    cmd_execute(cmd, dfs, &dfs_callback);
//}
//
//static uint64_t dfs_total(DfSingle *dfs) { return dfs->stat[DFS_TOTAL]; }
//
//static uint64_t dfs_used(DfSingle *dfs) { return dfs->stat[DFS_USED]; }
//
//static uint64_t dfs_avail(DfSingle *dfs) { return dfs->stat[DFS_AVAIL]; }
//
//static uint64_t dfs_use(DfSingle *dfs) { return dfs->stat[DFS_USE]; }
//
//
//enum
//{
//    BLK_NAME = 0,
//    BLK_FSTYPE,
//    BLK_SCHED,
//    BLK_SIZE,
//    BLK_MODEL,
//    BLK_LABEL,
//    BLK_UUID,
//    BLK_MOUNTPOINT,
//    BLK_LAST
//};
//
//typedef struct {
//
//} LsblkSignle;
//
//typedef struct
//{
//    char* ns;
//    char* ne;
//    char* vs;
//    char* ve;
//} kv_pair;
//
//enum
//{
//    SM_CHAR = 0,
//    SM_EQUAL,
//    SM_QUOTE,
//    SM_SPACE
//};
//
//
//static int sm_token(char ch)
//{
//    if(ch == '"') return SM_QUOTE;
//    if(ch == '=') return SM_EQUAL;
//    if(isspace(ch)) return SM_SPACE;
//    return SM_CHAR;
//}
//
//#define FSM_CALLBACK(x) ((size_t)&x)
//
//static void sblk_callback(slist* lines, void *ctx) {
//
////    printf(source);
////
////    string stat[BLK_LAST];
////
////    kv_pair m[BLK_LAST];
////    memset(m, 0, sizeof(m));
////
////
////
////    for(size_t i =0, j = 0; i < EXECUTE_LINE_BUFFER; ++i, ++j)
////    {
////        m[j].ne = &source[i];
////        const char ch = source[i];
////        int tok = sm_token(ch);
////
////
////    }
//
//
//}
//
//static int sblk_execute(LsblkSignle *sblk, const char *dev) {
//    static const char *options[] = {"NAME", "FSTYPE", "SCHED", "SIZE", "MODEL", "LABEL", "UUID", "MOUNTPOINT"};
//
//    string cmd;
//    string_create(&cmd, "lsblk -i -P -b -o ");
//    string_append(&cmd, options[0]);
//
//
//    size_t opt_size = sizeof(options) / sizeof(char *);
//    for (size_t i = 1; i < opt_size; ++i) {
//        string_append(&cmd, ",");
//        string_append(&cmd, options[i]);
//    }
//
//    string_append(&cmd, " ");
//    string_appendz(&cmd, dev);
//
//
//    char *ccmd = NULL;
//    size_t cmd_len;
//    string_get(&cmd, &ccmd, &cmd_len);
//    cmd_execute(ccmd, sblk, &sblk_callback);
//
//    return ST_OK;
//}
//

int main() {

    init_gloabls();
    test_slist();

    ht_destroy(alloc_table);

//    for(;;) {
//        test_slist();
//        usleep(1);
//
//    }




//    DfSingle dfs;
//
//    dfs_execute(&dfs, "/dev/sdb1");

//
//    LsblkSignle blk;
//    sblk_execute(&blk, "/dev/sdb1");

    return 0;
}

