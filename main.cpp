#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <regex>
#include <sstream>

// c standart headers
#include <stdio.h>
#include <unistd.h>

//directory headers
#include <dirent.h>

#include <err.h>
#include <blkid/blkid.h>

#include <sys/statvfs.h>
#include <set>
#include <cmath>
#include <iomanip>

#include <thread>

#include <ncurses.h>
#include <fcntl.h>

enum {
    ST_OK,
    ST_ERR
};

enum {
    /// These values increment when an I/O request completes.
            READ_IO = size_t(0), ///requests - number of read I/Os processed

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

class Status {
    int res_ = 0;
    std::string msg_;
public:

    Status() : res_(ST_OK) {}

    Status(int res) :
            res_(res) {
    }

    Status(int res, std::string msg)
            : res_(res), msg_(msg) {

    }

    Status(std::string msg)
            : res_(ST_ERR), msg_(msg) {

    }

    operator bool() {
        return res_ == ST_OK;
    }

    operator int() {
        return res_;
    }

    std::string what() { return msg_; }
};

template<typename T>
std::string tostring(const T &o) {
    std::stringstream ss;
    ss << o;
    return ss.str();
}

template<typename T>
std::string _format(std::string &&s, T &&value) {

    return std::regex_replace(s, std::regex("(%)"), tostring(value), std::regex_constants::format_first_only);
}

template<typename T, typename... Args>
std::string _format(std::string &&s, T &&value, Args... args) {
    std::string ss = std::regex_replace(s, std::regex("(%)"), tostring(value), std::regex_constants::format_first_only);

    return std::move(_format(std::move(ss), args...));
}

template<typename T, typename... Args>
std::string format(const char *fmt, T &&value, Args... args) {
    std::string s = fmt;
    return std::move(_format(std::move(s), std::move(value), args...));
}

std::vector<uint64_t> split_int_ws(const std::string &input) {
    std::vector<uint64_t> v;
    std::regex re("([0-9]+)");
    for (auto it = std::sregex_iterator(input.begin(), input.end(), re);
         it != std::sregex_iterator();
         ++it) {
        std::smatch m = *it;

        v.push_back(std::stoull(m.str()));
    }


    return v;
}

static inline std::string safe_string(const char *str) {
    if (str)
        return std::string(str);

    return std::string();
}


static int execute(const std::string &cmd, std::vector<std::string> &output) {
    FILE *fpipe;
    char line[2048];

    if (!(fpipe = popen(cmd.c_str(), "r"))) {
        return Status("Problems with pipe");
    }

    while (fgets(line, sizeof line, fpipe)) {
        output.emplace_back(line);
    }
    pclose(fpipe);
    return Status();
}


enum {
    DFS_TOTAL = uint64_t(0),
    DFS_USED,
    DFS_AVAIL,
    DFS_USE

};

class DfSingle {
    std::string cmd = "df --block-size=1";
    std::vector<uint64_t> stat;
public:
    void exec(const std::string &dev) {
        cmd += " " + dev;

        std::vector<std::string> lines;
        execute(cmd, lines);

        std::string data = lines[1];

        //std::cout<<std::endl<<data<<std::endl;
        std::regex re("([^\\w][0-9]+)");
        for (auto it = std::sregex_iterator(data.begin(), data.end(), re);
             it != std::sregex_iterator(); ++it) {
            std::smatch m = *it;
            //std::cout<<m.str()<<std::endl;

            stat.push_back(std::stoull(m.str()));
        }
    }

    uint64_t total() { return stat[DFS_TOTAL]; }

    uint64_t used() { return stat[DFS_USED]; }

    uint64_t avail() { return stat[DFS_AVAIL]; }

    uint64_t use() { return stat[DFS_USE]; }
};

class LsblkSignle {
    std::set<std::string> options = {"NAME", "FSTYPE", "SCHED", "SIZE", "MODEL", "LABEL", "UUID", "MOUNTPOINT"};
    std::string params = "-i -P -b";
    std::map<std::string, std::string> stat_;
public:

    void exec(const std::string &dev) {
        std::string cmd = "lsblk " + opttostr() + " " + params + " " + dev;

        std::vector<std::string> lines;
        execute(cmd, lines);

        std::string data = lines[0];

        //std::cout<<std::endl<<data<<std::endl;
        std::regex re("(\\w+)=\"([\\w\\[\\]/\\s-]*)\"");
        for (auto it = std::sregex_iterator(data.begin(), data.end(), re);
             it != std::sregex_iterator(); ++it) {
            std::smatch m = *it;
            stat_[m[1].str()] = m[2].str();
        }
    }


    std::string stat(const std::string &name) {
        return stat_[name];
    }

private:
    std::string opttostr() {
        std::string opt = "-o ";

        for (auto o : options) {
            opt += o + ",";
        }

        opt = opt.substr(0, opt.size() - 1);

        return opt;
    }
};

class Buffer {
public:
    virtual ~Buffer() = 0;

    Buffer() = default;

    Buffer(const Buffer &) = delete;

    Buffer &operator=(const Buffer &) = delete;

    Buffer(Buffer &&b) {
        buff_ = std::move(b.buff_);
        p = b.p;
        g = b.g;
    }

    Buffer &&operator=(Buffer &&b) {
        buff_ = std::move(b.buff_);
        p = b.p;
        g = b.g;

        return std::move(*this);
    }

    const char *data() const { return buff_.data(); }

    virtual void read(std::ifstream &fs) = 0;

    virtual void write(const std::string &data) = 0;

    virtual void write(const char *data, size_t size) = 0;

    virtual std::string read_all() const = 0;

protected:
    std::string buff_;
    size_t p = 0, g = 0;
};

Buffer::~Buffer() {}


class RWBuffer : public Buffer {
public:

    RWBuffer() = default;

    RWBuffer(const Buffer &) = delete;

    RWBuffer &operator=(const Buffer &) = delete;

    RWBuffer(RWBuffer &&b) = delete;

    RWBuffer &&operator=(RWBuffer &&b) = delete;

    void write(const std::string &data) override final {
        write(data.data(), data.size());
    }

    void write(const char *data, size_t size) override final {
        if (size + p >= buff_.size())
            buff_.resize((size + p) * 2);

        memcpy(&buff_[p], data, size);
        p += size;

    }

    void read(std::ifstream &fs) override final {
        fs.seekg(0, fs.end);
        size_t size = fs.tellg();
        fs.seekg(0, fs.beg);

        if ((size + p) >= buff_.size())
            buff_.resize((size + p) * 2);


        fs.read(&buff_[p], size);
        p += size;
    }

    std::string read_all() const override final {
        return buff_;
    }
};


static std::string read_all_file(const std::string &path) {
    auto ifs = std::ifstream(path, std::ios::in);
    if (!ifs)
        throw std::runtime_error("Cannot open for reading: " + path);

    RWBuffer buf;

    buf.read(ifs);

    ifs.close();

    return buf.read_all();
}

static std::vector<std::string> scan_dir(std::string basedir, std::regex re) {
    dirent *dir = nullptr;
    DIR *d = opendir(basedir.c_str());

    std::vector<std::string> v;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (std::regex_match(dir->d_name, re)) {
                std::stringstream ss;
                ss << basedir << "/" << std::string(dir->d_name);
                v.push_back(ss.str());
            }
        }

        closedir(d);
    }

    return v;
}

template<typename T>
std::string f2s(T v, int p) {
    std::stringstream ss;
    ss << std::setprecision(p) << std::fixed << v;
    return ss.str();
}


std::string hr_size(uint64_t bytes) {

    double r = bytes / 1024.;
    if (r < 1024)  // KB / sec
        return tostring((uint64_t) r) + "Kb";

    r = bytes / 1024 / 1024;
    if (r < 1024)  // MiB / sec
        return tostring((uint64_t) r) + "Mb";

    r = bytes / 1024 / 1024 / 1024;
    return tostring((uint64_t) r) + "Gb";
}

struct Device;
using device_ptr = std::shared_ptr<Device>;

struct Device {
    std::string name;
    struct statvfs stats;
    std::vector<uint64_t> stat;
    std::map<std::string, std::string> perf;
    std::string uuid;
    std::string label;
    uint64_t size = 0;
    uint64_t used = 0;
    uint64_t avail = 0;
    double perc = 0.0;
    std::string fsize;
    std::string fuse;
    std::string fs;
    std::string mount;
    std::string sysfolder;
    std::string model;
    bool child;
    std::vector<device_ptr> childs;


    device_ptr deepcopy() {
        device_ptr c = std::make_shared<Device>();
        c->name = name;
        c->stats = stats;
        c->stat = stat;
        c->perf = perf;
        c->uuid = uuid;
        c->label = label;
        c->size = size;
        c->used = used;
        c->avail = avail;
        c->perc = perc;
        c->fsize = fsize;
        c->fuse = fuse;
        c->fs = fs;
        c->mount = mount;
        c->sysfolder = sysfolder;
        c->model = model;
        c->child = child;
        c->childs.clear();

        for (auto dev : childs) {
            device_ptr cc = dev->deepcopy();
            c->childs.push_back(cc);
        }

        return c;

    }

    void debug() {
        std::stringstream vfs;
        vfs << "[f_bsize=" << stats.f_bsize << ", f_frsize=" << stats.f_frsize <<
            ", f_blocks=" << stats.f_blocks << ", f_bfree=" << stats.f_bfree << ", f_bavail=" <<
            stats.f_bavail << ", f_files=" << stats.f_files << ", f_ffree=" << stats.f_ffree <<
            "]";

        std::stringstream ss;
        ss << "name=" << name << std::endl;
        ss << "statvfs=" << vfs.str() << std::endl;
        ss << "read=" << perf["READ"] << std::endl;
        ss << "write=" << perf["WRITE"] << std::endl;
        ss << "uid=" << uuid << std::endl;
        ss << "label=" << label << std::endl;
        ss << "size=" << size << std::endl;
        ss << "used=" << used << std::endl;
        ss << "avail=" << avail << std::endl;
        ss << "perc=" << perc << std::endl;
        ss << "fs=" << fs << std::endl;
        ss << "mount=" << mount << std::endl;
        ss << "sysfolder=" << sysfolder << std::endl;
        ss << "model=" << model << std::endl;
        ss << "child=" << std::boolalpha << child << std::endl;

        std::cerr << ss.str() << std::endl;
    }
};

class DeviceManager {
    std::vector<device_ptr> base_devs;
public:
    void detect() {
        std::regex re1("^s.*$");
        auto vdev = scan_dir("/sys/block", re1);

        base_devs.resize(vdev.size());

        for (auto &dev : base_devs)
            dev = std::make_shared<Device>();

        for (size_t i = 0; i < vdev.size(); ++i) {
            auto pos = vdev[i].find_last_of('/');
            auto devn = vdev[i].substr(pos + 1);


            base_devs[i]->sysfolder = vdev[i];
            base_devs[i]->name = devn;
            base_devs[i]->child = false;

            auto childsv = scan_dir(base_devs[i]->sysfolder, std::regex(base_devs[i]->name + "[1-9]+$"));

            for (size_t j = 0; j < childsv.size(); ++j) {
                device_ptr child = std::make_shared<Device>();
                child->sysfolder = childsv[j];
                auto cpos = childsv[j].find_last_of('/');
                auto cdevn = childsv[j].substr(cpos + 1);
                child->name = cdevn;
                child->child = true;

                base_devs[i]->childs.push_back(child);
            }


        }
    }

    void enrich_devs() {
        for (auto dev : base_devs) {
            for (auto cdev : dev->childs) {
                enrich(cdev);
            }

            enrich(dev);


        }
    }

    std::vector<device_ptr> devs() {
        return base_devs;
    }

private:

    device_ptr get_dev_by_name(device_ptr base, const std::string &name) {
        if (base->name == name)
            return base;
        else if (base->childs.size() > 0) {
            for (size_t i = 0; i < base->childs.size(); ++i) {
                if (get_dev_by_name(base->childs[i], name))
                    return base->childs[i];
            }
        }

        return nullptr;
    }

    int enrich(device_ptr dev) {
        enrich_dev_stat(dev);
        enrich_etc(dev);
        enrich_size(dev);

        return ST_OK;
    }

    int enrich_etc(device_ptr dev) {
        LsblkSignle blk;
        blk.exec("/dev/" + dev->name);
        dev->fs = blk.stat("FSTYPE");
        dev->model = blk.stat("MODEL");
        dev->mount = blk.stat("MOUNTPOINT");
        dev->uuid = blk.stat("UUID");
        dev->label = blk.stat("LABEL");

        if (!dev->child) {
            dev->size = std::stoull(blk.stat("SIZE"));
            //dev->fsize = hr_size(dev->size);
        }

        return ST_OK;
    }

    int enrich_size(device_ptr dev) {
        DfSingle df;
        df.exec("/dev/" + dev->name);

        if (dev->child) {
            dev->size = df.total();
            dev->used = df.used();
            dev->avail = df.avail();
            dev->perc = dev->used / (double) dev->size * 100.0;
        } else {
            for (auto ch : dev->childs) {
                dev->used += ch->used;
            }


            dev->perc = dev->used / (double) dev->size * 100.0;
        }


        auto hr_used = hr_size(dev->used);
        auto hr_total = hr_size(dev->size);
        auto hr_use = f2s(dev->perc, 2);

        std::stringstream ss;
        ss << hr_used << "/" << hr_total;

        dev->fuse = tostring(hr_use) + "%";

        dev->fsize = ss.str();

        return ST_OK;
    }

    int enrich_dev_stat(device_ptr dev) {
        auto filename = dev->sysfolder + "/stat";
        auto data = read_all_file(filename);

        data = data.substr(0, data.find_last_of('\n'));

        std::regex re("([0-9]+)");

        std::vector<uint64_t> v;
        for (auto it = std::sregex_iterator(data.begin(), data.end(), re);
             it != std::sregex_iterator(); ++it) {
            std::smatch m = *it;
            uint64_t i = std::stoull(m.str());
            v.push_back(i);

        }

        dev->stat = std::move(v);

        return ST_OK;
    }
};

class PerfMeter {

    uint32_t sample_size_;
    const size_t BLOCK_SIZE = 512; // Unix block size
public:
    PerfMeter(uint32_t sample_size)
            : sample_size_(sample_size) {
    }

    uint32_t sample_size() const { return sample_size_; }

    std::vector<device_ptr> measure() {
        DeviceManager dm1;
        dm1.detect();
        dm1.enrich_devs();

        std::this_thread::sleep_for(std::chrono::seconds(sample_size_));

        DeviceManager dm2;
        dm2.detect();
        dm2.enrich_devs();

        std::vector<device_ptr> devs1 = dm1.devs();
        std::vector<device_ptr> devs2 = dm2.devs();

        if (devs1.size() != devs2.size())
            std::cerr << "Integrity of devices is corrupted" << std::endl;

        std::vector<device_ptr> devs3;
        devs3.resize(devs2.size());
        for (size_t i = 0; i < devs2.size(); ++i) {
            devs3[i] = diff(devs1[i], devs2[i]);
        }

        return devs3;
    }


private:
    std::string human_readable(uint64_t bytes) {

        float r = bytes / 1024.f / sample_size_;
        if (r < 1024)  // KB / sec
            return f2s(r, 3) + " Kb/s";

        r = bytes / 1024 / 1024 / sample_size_;
        if (r < 1024)  // MiB / sec
            return f2s(r, 3) + " Mb/s";

        r = bytes / 1024 / 1024 / 1024 / sample_size_;
        return f2s(r, 3) + " Gb/s";
    }

    inline device_ptr diff(device_ptr a, device_ptr b) {
        auto c = b->deepcopy();
        c->stat[WRITE_SECTORS] = b->stat[WRITE_SECTORS] - a->stat[WRITE_SECTORS];
        c->stat[READ_SECTORS] = b->stat[READ_SECTORS] - a->stat[READ_SECTORS];
        c->perf["READ"] = human_readable(c->stat[READ_SECTORS] * BLOCK_SIZE);
        c->perf["WRITE"] = human_readable(c->stat[WRITE_SECTORS] * BLOCK_SIZE);

        for (size_t i = 0; i < b->childs.size(); ++i) {
            auto cdev = c->childs[i];
            auto adev = a->childs[i];
            cdev->stat[WRITE_SECTORS] = cdev->stat[WRITE_SECTORS] - adev->stat[WRITE_SECTORS];
            cdev->stat[READ_SECTORS] = cdev->stat[READ_SECTORS] - adev->stat[READ_SECTORS];
            cdev->perf["READ"] = human_readable(cdev->stat[READ_SECTORS] * BLOCK_SIZE);
            cdev->perf["WRITE"] = human_readable(cdev->stat[WRITE_SECTORS] * BLOCK_SIZE);

        }

        return c;

    }
};

static int statfs_dev() {
    const char *dev = "/usr/bin/gcc";
    struct statvfs64 fs;

    int res = statvfs64(dev, &fs);

    const uint64_t total = fs.f_blocks * fs.f_frsize;
    const uint64_t available = fs.f_bfree * fs.f_frsize;
    const uint64_t used = total - available;
    const double usedPercentage = ceil(used / total * 100.0);


    std::cout << dev << " " << std::fixed << total << " " << used << " " << available << " " << usedPercentage << "%"
              << std::endl;

    return 0;
}


struct Colon {
    static const int OFFSET = 1;
    static const int DEVICE = OFFSET;
    static const int READ = 9 + OFFSET;
    static const int WRITE = 22 + OFFSET;
    static const int SIZE = 35 + OFFSET;
    static const int USE = 50 + OFFSET;
    static const int FILESYSTEM = 58 + OFFSET;
    static const int MOUNT = 64 + OFFSET;
    static const int MODEL = 80 + OFFSET;
};

class Row {
    int row = 4;

public:
    int operator++(int) {
        return row++;
    }

    int operator++() {
        return ++row;
    }

    operator int() {
        return row;
    }
};

int ncurses_windows() {
    int mrow = 0, mcol = 0;
    initscr();            /* Start curses mode 		  */

    if (has_colors() == FALSE) {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }

    //noecho();
    //cbreak();
    start_color();

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);

    getmaxyx(stdscr, mrow, mcol);

    while (1) {
        Row row;

        PerfMeter pm(1);

        auto devs = pm.measure();
        clear();

        attron(A_BOLD);
        attron(COLOR_PAIR(1));

        mvaddstr(1, 1, "HWMonitor 0.1a\n");

        std::stringstream ssize;
        ssize << "Sample size " << pm.sample_size() << "\n\n";
        mvaddstr(2, 1, ssize.str().c_str());


        mvaddstr(row, Colon::DEVICE, "Device");
        mvaddstr(row, Colon::READ, "Read");
        mvaddstr(row, Colon::WRITE, "Write");
        mvaddstr(row, Colon::SIZE, "Size");
        mvaddstr(row, Colon::USE, "Use");
        mvaddstr(row, Colon::FILESYSTEM, "FS");
        mvaddstr(row, Colon::MOUNT, "Mount");
        mvaddstr(row++, Colon::MODEL, "Model");
        attroff(COLOR_PAIR(1));


        for (auto dev : devs) {
            attron(COLOR_PAIR(2));

            mvaddstr(row, Colon::DEVICE, dev->name.c_str());
            mvaddstr(row, Colon::READ, dev->perf["READ"].c_str());
            mvaddstr(row, Colon::WRITE, dev->perf["WRITE"].c_str());
            mvaddstr(row, Colon::SIZE, dev->fsize.c_str());
            mvaddstr(row, Colon::USE, dev->fuse.c_str());
            mvaddstr(row, Colon::FILESYSTEM, dev->fs.c_str());
            mvaddstr(row, Colon::MOUNT, dev->mount.c_str());
            mvaddstr(row++, Colon::MODEL, dev->model.c_str());

            attroff(COLOR_PAIR(2));
            attron(COLOR_PAIR(3));
            for (auto child : dev->childs) {
                mvaddstr(row, Colon::DEVICE, child->name.c_str());
                mvaddstr(row, Colon::READ, child->perf["READ"].c_str());
                mvaddstr(row, Colon::WRITE, child->perf["WRITE"].c_str());
                mvaddstr(row, Colon::SIZE, child->fsize.c_str());
                mvaddstr(row, Colon::USE, child->fuse.c_str());
                mvaddstr(row, Colon::FILESYSTEM, child->fs.c_str());
                mvaddstr(row++, Colon::MOUNT, child->mount.c_str());
            }

            attroff(COLOR_PAIR(3));
        }

        attroff(A_BOLD);

        refresh();            /* Print it on to the real screen */
    }

    getch();
    endwin();
}


enum class PoArg {
    NO_NCURSES,
    DEBUG,
    NET
};


std::map<PoArg, std::string> po_arg_parse(int argc, char **argv) {
    std::map<PoArg, std::string> opts;

    std::regex re("([tdn])");
    std::string args;
    for (int i = 1; i < argc; ++i)
        args += std::string(argv[i]) + " ";

    for (auto it = std::sregex_iterator(args.begin(), args.end(), re);
         it != std::sregex_iterator();
         ++it) {
        std::smatch m = *it;
        if (m.str() == "t") opts[PoArg::NO_NCURSES] = "";
        if (m.str() == "d") opts[PoArg::DEBUG] = "";
        if (m.str() == "n") opts[PoArg::NET] = "";
    }

    return opts;
}

int text_windows() {
    for(;;) {
        PerfMeter pm(1);

        auto devs = pm.measure();


        for (auto dev : devs) {

            std::cout << dev->name << "\t" <<
                      dev->perf["READ"] << "\t" <<
                      dev->perf["WRITE"] << "\t" <<
                      dev->fsize << '\t' <<
                      dev->fuse << '\t' <<
                      dev->fs << '\t' <<
                      dev->mount << '\t' <<
                      dev->model << std::endl;

            for (auto child : dev->childs) {

                std::cout << child->name << "\t" <<
                          child->perf["READ"] << "\t" <<
                          child->perf["WRITE"] << "\t" <<
                          child->fsize << '\t' <<
                          child->fuse << '\t' <<
                          child->fs << '\t' <<
                          child->mount << '\t' <<
                          child->model << std::endl;
            }

        }

    }
}

int main(int argc, char **argv) {

    auto opts = po_arg_parse(argc, argv);

    if (opts.find(PoArg::NO_NCURSES) != opts.end()) {
        text_windows();
    } else {
        ncurses_windows();
    }

    return 0;

}
