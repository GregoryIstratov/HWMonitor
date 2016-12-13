// c standart headers
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <math.h>

//directory headers
#include <dirent.h>

#include <err.h>
#include <errno.h>
#include <blkid/blkid.h>

#include <sys/statvfs.h>

#include <pthread.h>

#include <ncurses.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>


#define EXIT_ERROR(msg) { fprintf(stderr, "%s:%d [%s]: %s",  __FILE__, __LINE__, __PRETTY_FUNCTION__, msg); exit(EXIT_FAILURE); }
#define ASSERT(x) { if(x) { fprintf(stderr, "%s:%d [%s]: assertion failed [%s]",  __FILE__, __LINE__, __PRETTY_FUNCTION__, ##x); exit(EXIT_FAILURE); }}
#define EXIT_NULL(x) { if(##x) { fprintf(stderr, "%s:%d [%s]: returns NULL pointer",  __FILE__, __LINE__, __PRETTY_FUNCTION__); exit(EXIT_FAILURE); }}

//===============================================================
#define EXECUTE_LINE_BUFFER 2048
#define READ_BUFF_SIZE 512
#define STRING_INIT_BUFFER 32

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

//===============================================================


typedef struct {
    char *ptr;
    size_t size;
    size_t used;
    size_t mul;

} dynamic_allocator;


static int da_init(dynamic_allocator *a) {
    memset(a, 0, sizeof(dynamic_allocator));

    EXIT_NULL((a->ptr = malloc(STRING_INIT_BUFFER));
    a->size = STRING_INIT_BUFFER;
    a->mul = 1;

    return 0;
}

static int da_init_n(dynamic_allocator *a, size_t size) {
    memset(a, 0, sizeof(dynamic_allocator));

    EXIT_NULL((a->ptr = malloc(size));
    a->size = size;
    a->mul = 1;

    return 0;
}

static int da_release(dynamic_allocator *a) {
    free(a->ptr);
    memset(a, 0, sizeof(dynamic_allocator));

    return 0;
}

static int da_reallocate(dynamic_allocator *a, size_t size) {
    a->size += size * a->mul;
    EXIT_NULL(realloc(a->ptr, a->size));

    return 0;
}

static int da_shink(dynamic_allocator *a) {
    EXIT_NULL(realloc(a->ptr, a->used));
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

    memmove(a->ptr + a->used, data, size);
    a->used += size;
}

static int da_merge(dynamic_allocator *a, dynamic_allocator *b) {
    da_shink(a);
    da_shink(b);

    size_t nb_size = a->size + b->size;

    da_reallocate(a, nb_size);
    a->size = nb_size;

    memmove(a->ptr, b->ptr, b->size);

    a->used += b->size;


    return ST_OK;
}


//==============================================================

typedef struct {
    dynamic_allocator alloc;
    size_t size;
    static const size_t npos;
} string;

const size_t string::npos = (size_t)-1;

static int string_init(string *s) {
    da_init(&s->alloc);
    s->size = 0;

    return ST_OK;
}

static int string_init_n(string *s, size_t size) {
    da_init_n(&s->alloc, size);
    s->size = 0;

    return ST_OK;
}

static int string_append(string *s, const char *str) {
    size_t len = strlen(str);
    s->size += len;
    da_append(&s->alloc, str, len);

    return ST_OK;

}

static int string_appendn(string *s, const char *str, size_t len) {
    s->size += len;
    da_append(&s->alloc, str, len);

    return ST_OK;

}

static int string_create(string *s, const char *str) {
    string_init(s);

    string_append(s, str);

    return ST_OK;
}

static int string_make_nullterminated(string *s) {
    return string_append(s, "\0");
}

static int string_get(string *s, char **str, size_t *size) {
    *str = malloc(s->size);
    memmove(*str, s->alloc.ptr, s->size);
    *size = s->size;

    return ST_OK;
}

static char string_get_ch(string* s, size_t idx)
{
    if(idx > 0 && idx <= s->size)
        EXIT_ERROR("Index is out of bound");

    return s->alloc.ptr[idx];
}

static int string_add(string *a, string *b) {
    da_merge(&a->alloc, &b->alloc);
    a->size += b->size;

    return ST_OK;
}

static size_t string_find_last_char(string* s, char ch)
{
    for(size_t i = s->size; i != 0; --i)
    {
        char cur = s->alloc.ptr[i];
        if(cur == ch)
            return i;
    }

    return string::npos;

}


static void string_map_region(string* s, size_t beg, size_t end, char** sb, char** se)
{
    if((beg > 0 && beg <= s->size) && (end > 0 && end <= s->size))
        EXIT_ERROR("Indexes are out of bound");

    *sb = s->alloc.ptr + beg;
    *se = s->alloc.ptr + end;
}

static void string_map_string(string* s, char** sb, char** se)
{
    string_map_region(s, 0, s->size, sb, se);
}

static void string_print(string *a) {
    //printf("%.*s", a->size, a->alloc.ptr);
    fwrite(a->alloc.ptr, 1, a->size, stdout);
    fflush(stdout);
}

//======================================================================

#define ERROR_EXIT() { perror(strerror(errno)); exit(errno); }

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


static uint64_t str2ull(const char *str, size_t size) {
    uint64_t res = 0;

    for (size_t i = 0; i < size; ++i)
        res = res * 10 + str[i] - '0';

    return res;
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


size_t get_file_size(const char* filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

typedef void(*cmd_exec_cb)(string* s, void *ctx);

static void read_file_mmap(const char* filename, string* s)
{
    size_t filesize = get_file_size(filename);
    //Open file
    int fd = open(filename, O_RDONLY, 0);
    ASSERT((fd != -1))

    //Execute mmap
    void* data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    ASSERT(data != MAP_FAILED);

    //Write the mmapped data
    string_init_n(&s, filesize);
    string_appendn(&s, (char*)data, filesize);

    //Cleanup
    int rc = munmap(data, filesize);
    ASSERT(rc == 0);
    close(fd);
}

/*


static int cmd_execute(const char *cmd, void *ctx, cmd_exec_cb cb) {
    FILE *fpipe;
    char buff[READ_BUFF_SIZE] = {0};

    if (!(fpipe = popen(cmd, "r"))) ERROR_EXIT();

    while (fgets(buff, sizeof(buff), fpipe)) {
        string s
        cb(line, ctx);
    }

    pclose(fpipe);

    return ST_OK;
}
*/

enum {
    DFS_TOTAL = 0,
    DFS_USED = 1,
    DFS_AVAIL = 2,
    DFS_USE = 3

};

typedef struct {
    uint64_t stat[4];
    int skip_first;
} DfSingle;


static void dfs_init(DfSingle *dfs) {
    memset(dfs, 0, sizeof(DfSingle));
    dfs->skip_first = 1;
}

static void dfs_callback(const char source[EXECUTE_LINE_BUFFER], void *ctx) {
    DfSingle *dfs = (DfSingle *) ctx;

    if (dfs->skip_first) {
        dfs->skip_first = 0;
        return;
    }

    regex_t re;
    int reti = regcomp(&re, "([/a-zA-Z1-9]*)\\s*([0-9]*)\\s*([0-9]*)\\s*([0-9]*)\\s*([0-9]*)%\\s*([/a-zA-Z1-9-]*)",
                       REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        exit(EXIT_FAILURE);
    }

    printf(&source[0]);

    static const size_t MAX_MATHCHES = 7;
    static const size_t MAX_GROUPS = 7;

    size_t nmatch = MAX_MATHCHES;
    regmatch_t pmatch[MAX_GROUPS];
    int rc;

    if (0 != (rc = regexec(&re, source, nmatch, pmatch, 0))) {
        printf("Failed to match '%s',returning %d.\n", source, rc);
        exit(EXIT_FAILURE);
    } else {

        size_t j = 0;
        //skip dev name and mount point
        for (size_t i = 2; i < MAX_GROUPS - 1; ++i, ++j) {

            size_t msize = (size_t) (pmatch[i].rm_eo - pmatch[i].rm_so);
            const char *vals = &source[pmatch[i].rm_so];

            dfs->stat[j] = str2ull(vals, msize);

//
//            printf("With the whole expression, "
//                           "a matched substring \"%.*s\" is found at position %d to %d.\n",
//                   pmatch[i].rm_eo - pmatch[i].rm_so, &source[pmatch[i].rm_so],
//                   pmatch[i].rm_so, pmatch[i].rm_eo - 1);

        }
    }


    regfree(&re);

}

static void dfs_execute(DfSingle *dfs, const char *dev) {

    string s;
    string_create(&s, "df --block-size=1");
    string_append(&s, " ");
    string_append(&s, dev);

    char *cmd = NULL;
    size_t cmd_len;
    string_get(&s, &cmd, &cmd_len);
    cmd_execute(cmd, dfs, &dfs_callback);
}

static uint64_t dfs_total(DfSingle *dfs) { return dfs->stat[DFS_TOTAL]; }

static uint64_t dfs_used(DfSingle *dfs) { return dfs->stat[DFS_USED]; }

static uint64_t dfs_avail(DfSingle *dfs) { return dfs->stat[DFS_AVAIL]; }

static uint64_t dfs_use(DfSingle *dfs) { return dfs->stat[DFS_USE]; }


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

typedef struct {

} LsblkSignle;

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

static void sblk_callback(const char source[EXECUTE_LINE_BUFFER], void *ctx) {

    printf(source);

    string stat[BLK_LAST];

    kv_pair m[BLK_LAST];
    memset(m, 0, sizeof(m));



    for(size_t i =0, j = 0; i < EXECUTE_LINE_BUFFER; ++i, ++j)
    {
        m[j].ne = &source[i];
        const char ch = source[i];
        int tok = sm_token(ch);


    }


}

static int sblk_execute(LsblkSignle *sblk, const char *dev) {
    static const char *options[] = {"NAME", "FSTYPE", "SCHED", "SIZE", "MODEL", "LABEL", "UUID", "MOUNTPOINT"};

    string cmd;
    string_create(&cmd, "lsblk -i -P -b -o ");
    string_append(&cmd, options[0]);


    size_t opt_size = sizeof(options) / sizeof(char *);
    for (size_t i = 1; i < opt_size; ++i) {
        string_append(&cmd, ",");
        string_append(&cmd, options[i]);
    }

    string_append(&cmd, " ");
    string_append(&cmd, dev);


    char *ccmd = NULL;
    size_t cmd_len;
    string_get(&cmd, &ccmd, &cmd_len);
    cmd_execute(ccmd, sblk, &sblk_callback);

}


int main() {
//    DfSingle dfs;
//
//    dfs_execute(&dfs, "/dev/sdb1");


    LsblkSignle blk;
    sblk_execute(&blk, "/dev/sdb1");

    return 0;
}
//


//std::vector <std::string> lines;
//execute(cmd, lines);
//
//std::string data = lines[0];
//
////std::cout<<std::endl<<data<<std::endl;
//std::regex re("(\\w+)=\"([\\w\\[\\]/\\s-]*)\"");
//for (
//auto it = std::sregex_iterator(data.begin(), data.end(), re);
//it !=
//
//std::sregex_iterator();
//
//++it) {
//std::smatch m = *it;
//stat_[m[1].
//
//str()
//
//] = m[2].
//
//str();
//
//}

//}

//
//    std::string stat(const std::string &name) {
//        return stat_[name];
//    }
//
//private:
//    std::string opttostr() {
//        std::string opt = "-o ";
//
//        for (auto o : options) {
//            opt += o + ",";
//        }
//
//        opt = opt.substr(0, opt.size() - 1);
//
//        return opt;
//    }
//};
//
//static int read_all_file(const char* path) {
//    FILE* f;
//
//    if(!(f = fopen(path, "r"))
//        perror()
//
//    RWBuffer buf;
//
//    buf.read(ifs);
//
//    fclose(f);
//
//    return buf.read_all();
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
//
//
//
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
//struct Device;
//using device_ptr = std::shared_ptr<Device>;
//
//struct Device {
//    std::string name;
//    struct statvfs stats;
//    std::vector<uint64_t> stat;
//    std::map<std::string, std::string> perf;
//    std::string uuid;
//    std::string label;
//    uint64_t size = 0;
//    uint64_t used = 0;
//    uint64_t avail = 0;
//    double perc = 0.0;
//    std::string fsize;
//    std::string fuse;
//    std::string fs;
//    std::string mount;
//    std::string sysfolder;
//    std::string model;
//    bool child;
//    std::vector<device_ptr> childs;
//
//
//    device_ptr deepcopy() {
//        device_ptr c = std::make_shared<Device>();
//        c->name = name;
//        c->stats = stats;
//        c->stat = stat;
//        c->perf = perf;
//        c->uuid = uuid;
//        c->label = label;
//        c->size = size;
//        c->used = used;
//        c->avail = avail;
//        c->perc = perc;
//        c->fsize = fsize;
//        c->fuse = fuse;
//        c->fs = fs;
//        c->mount = mount;
//        c->sysfolder = sysfolder;
//        c->model = model;
//        c->child = child;
//        c->childs.clear();
//
//        for (auto dev : childs) {
//            device_ptr cc = dev->deepcopy();
//            c->childs.push_back(cc);
//        }
//
//        return c;
//
//    }
//
//    void debug() {
//        std::stringstream vfs;
//        vfs << "[f_bsize=" << stats.f_bsize << ", f_frsize=" << stats.f_frsize <<
//            ", f_blocks=" << stats.f_blocks << ", f_bfree=" << stats.f_bfree << ", f_bavail=" <<
//            stats.f_bavail << ", f_files=" << stats.f_files << ", f_ffree=" << stats.f_ffree <<
//            "]";
//
//        std::stringstream ss;
//        ss << "name=" << name << std::endl;
//        ss << "statvfs=" << vfs.str() << std::endl;
//        ss << "read=" << perf["READ"] << std::endl;
//        ss << "write=" << perf["WRITE"] << std::endl;
//        ss << "uid=" << uuid << std::endl;
//        ss << "label=" << label << std::endl;
//        ss << "size=" << size << std::endl;
//        ss << "used=" << used << std::endl;
//        ss << "avail=" << avail << std::endl;
//        ss << "perc=" << perc << std::endl;
//        ss << "fs=" << fs << std::endl;
//        ss << "mount=" << mount << std::endl;
//        ss << "sysfolder=" << sysfolder << std::endl;
//        ss << "model=" << model << std::endl;
//        ss << "child=" << std::boolalpha << child << std::endl;
//
//        std::cerr << ss.str() << std::endl;
//    }
//};
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
//        LsblkSignle blk;
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
//        DfSingle df;
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
//
//int main(int argc, char **argv) {
//
//    auto opts = po_arg_parse(argc, argv);
//
//    if (opts.find(PoArg::NO_NCURSES) != opts.end()) {
//        text_windows();
//    } else {
//        ncurses_windows();
//    }
//
//    return 0;
//
//}
