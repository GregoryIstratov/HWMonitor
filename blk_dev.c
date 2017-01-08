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
#include <dirent.h>
#include <stdatomic.h>
#include "blk_dev.h"
#include "allocators.h"
#include "utils.h"
#include "log.h"
#include "timer.h"

//============================================================================================================
// BLOCK DEVICE MANAGMENT
//============================================================================================================

void blk_dev_release_cb(void* p) {
    blk_dev_t* dev = (blk_dev_t*)p;

    if (dev->name)
        string_release(dev->name);
    if (dev->label)
        string_release(dev->label);
    if (dev->fs)
        string_release(dev->fs);
    if (dev->mount)
        string_release(dev->mount);
    if (dev->sysfolder)
        string_release(dev->sysfolder);
    if (dev->model)
        string_release(dev->model);
    if (dev->uuid)
        string_release(dev->uuid);
    if (dev->shed)
        string_release(dev->shed);

    zfree(dev);
}

blk_dev_t* blk_dev_list_search(list_t* devs, string* name) {
    list_iter_t* it = NULL;
    list_iter_init(devs, &it);

    blk_dev_t* dev;
    while ((dev = list_iter_next(it))) {
        string* dev_devname = NULL;
        string_create(&dev_devname, "/dev/");
        string_add(dev_devname, dev->name);

        if (string_compare(dev_devname, name) == ST_OK) {
            string_release(dev_devname);
            break;
        }

        string_release(dev_devname);
    }

    list_iter_release(it);

    return dev;
}

blk_dev_t* blk_dev_list_direct_search(list_t* devs, string* name) {
    list_iter_t* it = NULL;
    list_iter_init(devs, &it);

    blk_dev_t* dev;
    while ((dev = list_iter_next(it))) {

        if (string_compare(dev->name, name) == ST_OK)
            break;
    }

    list_iter_release(it);

    return dev;
}

void blk_dev_diff(blk_dev_t* __restrict a, blk_dev_t* __restrict b, double sample_size) {
// Unix block size
#define BLOCK_SIZE 512.0

    b->stat[WRITE_SECTORS] = b->stat[WRITE_SECTORS] - a->stat[WRITE_SECTORS];
    b->stat[READ_SECTORS] = b->stat[READ_SECTORS] - a->stat[READ_SECTORS];

    b->perf_read = b->stat[READ_SECTORS] * BLOCK_SIZE / sample_size;
    b->perf_write = b->stat[WRITE_SECTORS] * BLOCK_SIZE / sample_size;

#undef BLOCK_SIZE
}


//============================================================================================================
// DF UTILS
//============================================================================================================

void df_init(list_t* devs, df_t** df) {
    *df = zalloc(sizeof(df_t));
    (*df)->devs = devs;
    (*df)->skip_first = 1;
}

void df_callback(void* ctx, list_t* lines) {
    df_t* df = (df_t*)ctx;

    LOG_TRACE("------- LINES OF TOKENS -----------");
    slist_fprintd(lines);

    string* tk = NULL;
    list_iter_t* line_it = NULL;
    list_iter_init(lines, &line_it);
    while ((tk = slist_next(line_it))) {

        LOG_TRACE("------- TOKEN LINE-----------");
        string_printt(tk);

        if (!string_re_match(tk, ".*(sd.*).*"))
            continue;

        LOG_TRACE("------- TOKEN SPLIT-----------");

        list_t* tokens = NULL;
        string_split(tk, ' ', &tokens);

        slist_fprintd(tokens);

        list_iter_t* tk_it = NULL;
        list_iter_init(tokens, &tk_it);

        string* s = NULL;
        u64 k = 0;
        blk_dev_t* dev = NULL;
        while ((s = slist_next(tk_it)) != NULL) {

            string_strip(s);

            if (k == DFS_NAME) {
                dev = blk_dev_list_search(df->devs, s);
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
                        dev->perc = dev->used / (double)dev->size * 100.0;
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

void df_execute(df_t* dfs) {

    const char* cmd = "df --block-size=1";
    cmd_execute(cmd, dfs, &df_callback);
}

//============================================================================================================
// BLK UTILS
//============================================================================================================

ret_t vector_add_kv(vector_t* vec, string* key, string* val) {
    skey_value_t* kv = zalloc(sizeof(skey_value_t));
    string_dub(key, &kv->key);
    string_dub(val, &kv->value);

    vector_add(vec, kv);

    return ST_OK;
}

typedef struct string_string_pair {
    string* key;
    string* val;
} ss_kv_t;

static void ss_kv_release_cb(void* p) {
    ss_kv_t* kv = (ss_kv_t*)p;
    string_release(kv->key);
    string_release(kv->val);
    zfree(kv);
}

void sblk_callback(void* ctx, list_t* lines) {

    sblkid_t* sys = (sblkid_t*)ctx;

    string* tk = NULL;
    list_iter_t* ln_it = NULL;
    list_iter_init(lines, &ln_it);

    while ((tk = slist_next(ln_it))) {

        LOG_DEBUG("------- TOKEN LINE-----------");
        string_printt(tk);


        //====================================================================================================
        regex_t re;
        regex_compile(&re, "(\\w+)=\"([[:alnum:][:space:]/-]*)\"");

        char* tkp = NULL;
        string_create_nt(tk, &tkp);

        blk_dev_t* dev = NULL;
        list_t* pairs = NULL;
        list_init(&pairs, &ss_kv_release_cb);

        const char* p = tkp;
        /* "N_matches" is the maximum number of matches allowed. */
#define n_matches 5
        /* "M" contains the matches found. */
        regmatch_t m[n_matches];
        while (1) {
            int nomatch = regexec(&re, p, n_matches, m, 0);
            if (nomatch) {
                LOG_DEBUG("No more matches.");
                break;
            }

            string* key = NULL;
            string* val = NULL;
            ss_kv_t* ss_kv = zalloc(sizeof(ss_kv_t));
            for (int i = 0; i < n_matches; i++) {
                int start;
                int finish;
                if (m[i].rm_so == -1) {
                    break;
                }
#undef n_matches
                start = (int)(m[i].rm_so + (p - tkp));
                finish = (int)(m[i].rm_eo + (p - tkp));
                if (i == 0) {
                    continue;
                }
                if (i == 1) {
                    string_init(&key);
                    string_appendn(key, tkp + start, (u64)(finish - start));
                    ss_kv->key = key;

                } else if (i == 2) {

                    if (finish - start != 0) {

                        string_init(&val);
                        string_appendn(val, tkp + start, (u64)(finish - start));
                        ss_kv->val = val;

                        list_push(pairs, ss_kv);
                    } else {
                        string_release(key);
                        zfree(ss_kv);
                    }
                }

            }
            p += m[0].rm_eo;
        }

        regfree(&re);
        zfree(tkp);


        //====================================================================================================
        list_iter_t* kv_it = NULL;
        list_iter_init(pairs, &kv_it);

        ss_kv_t* kv;
        while ((kv = list_iter_next(kv_it))) {

            string_printd(kv->key);
            string_printd(kv->val);

            if (string_comparez(kv->key, "NAME") == ST_OK) {
                dev = blk_dev_list_direct_search(sys->devs, kv->val);
            } else if (string_comparez(kv->key, "SCHED") == ST_OK) {
                if (dev)
                    string_dub(kv->val, &dev->shed);

            } else if (string_comparez(kv->key, "FSTYPE") == ST_OK) {
                if (dev)
                    string_dub(kv->val, &dev->fs);

            } else if (string_comparez(kv->key, "MODEL") == ST_OK) {
                if (dev) {
                    string_strip(kv->val);
                    string_dub(kv->val, &dev->model);
                }
            } else if (string_comparez(kv->key, "MOUNTPOINT") == ST_OK) {
                if (dev)
                    string_dub(kv->val, &dev->mount);
            } else if (string_comparez(kv->key, "UUID") == ST_OK) {
                if (dev)
                    string_dub(kv->val, &dev->uuid);
            } else if (string_comparez(kv->key, "LABEL") == ST_OK) {
                if (dev)
                    string_dub(kv->val, &dev->label);
            } else if (string_comparez(kv->key, "SIZE") == ST_OK) {
                if (dev && dev->size == 0) {
                    string_to_u64(kv->val, &dev->size);
                }
            }

        }

        list_iter_release(kv_it);
        list_release(pairs, true);

        //====================================================================================================
    }

    list_iter_release(ln_it);
    list_release(lines, true);

}

ret_t sblk_execute(sblkid_t* sblk) {
    const char* options[] = {"NAME", "FSTYPE", "SCHED", "SIZE", "MODEL", "LABEL", "UUID",
                             "MOUNTPOINT"};

    string* cmd = NULL;
    string_create(&cmd, "lsblk -i -P -b -o ");
    string_append(cmd, options[0]);

    u64 opt_size = sizeof(options) / sizeof(char*);
    for (u64 i = 1; i < opt_size; ++i) {
        string_append(cmd, ",");
        string_append(cmd, options[i]);
    }

    string_append(cmd, "");

    char* ccmd = string_makez(cmd);
    cmd_execute(ccmd, sblk, &sblk_callback);

    string_release(cmd);
    zfree(ccmd);
    return ST_OK;
}

//============================================================================================================
// BLOCK DEVICE SCANNER
//============================================================================================================

void blk_dev_scan(string* basedir, list_t* devs) {
    struct dirent* dir = NULL;

    char* dir_c = string_makez(basedir);
    DIR* d = opendir(dir_c);
    zfree(dir_c);

    if (d) {
        while ((dir = readdir(d)) != NULL) {

            string* dir_name = NULL;
            string_create(&dir_name, dir->d_name);

            bool match = string_re_match(dir_name, "sd.*");

            if (match) {

                blk_dev_t* dev = zalloc(sizeof(blk_dev_t));

                // create sysdir
                string* sysdir = NULL;
                string_init(&sysdir);
                string_add(sysdir, basedir);
                string_add(sysdir, dir_name);
                string_append(sysdir, "/");

                // set name and sysdir
                string_dub(dir_name, &dev->name);
                dev->sysfolder = sysdir;

                // getting stat
                string* stat_s = NULL;
                string_init(&stat_s);

                string* stat_filename = NULL;
                string_dub(sysdir, &stat_filename);
                string_append(stat_filename, "stat");

                char* stat_filename_c = string_makez(stat_filename);
                file_read_all_s(stat_filename_c, stat_s);
                string_strip(stat_s);

                zfree(stat_filename_c);
                string_release(stat_filename);

                list_t* lstat_s = NULL;
                string_split(stat_s, ' ', &lstat_s);

                string_release(stat_s);

                list_iter_t* lstat_it = NULL;
                list_iter_init(lstat_s, &lstat_it);

                string* s;
                u64 stat_n = 0;
                while ((s = (string*)list_iter_next(lstat_it)))
                    string_to_u64(s, &dev->stat[stat_n++]);

                list_iter_release(lstat_it);
                list_release(lstat_s, true);


                // add dev to list
                list_push(devs, dev);

                string* subdir = NULL;
                string_dub(sysdir, &subdir);

                // recursive iterate
                blk_dev_scan(subdir, devs);

                string_release(subdir);
            }

            string_release(dir_name);
        }

        closedir(d);
    }
}

//============================================================================================================
// BLOCK DEVICE SAMPLING
//============================================================================================================

void blkdev_get(list_t** devs) {
    string* basedir = NULL;

    list_init(devs, &blk_dev_release_cb);

    string_create(&basedir, "/sys/block/");

    blk_dev_scan(basedir, *devs);

    string_release(basedir);

    df_t* df;
    df_init(*devs, &df);
    df_execute(df);
    zfree(df);

    sblkid_t blk;
    blk.devs = *devs;
    sblk_execute(&blk);

#ifndef NDEBUG
    list_iter_t* list_it = NULL;
    list_iter_init(*devs, &list_it);

    blk_dev_t* dev;
    while ((dev = (blk_dev_t*)list_iter_next(list_it))) {
        char* name = string_makez(dev->name);
        char* syspath = string_makez(dev->sysfolder);
        char* fs = string_makez(dev->fs);
        char* model = string_makez(dev->model);
        char* mount = string_makez(dev->mount);
        char* uuid = string_makez(dev->uuid);
        char* label = string_makez(dev->label);
        LOG_DEBUG("[0x%p][name=%s][syspath=%s][size=%lu][used=%lu][avail=%lu][use=%lu][perc=%lf][fs=%s]"
                          "[model=%s][mount=%s][uuid=%s][label=%s]\n",
                  (void*)dev, name, syspath,
                  dev->size, dev->used, dev->avail, dev->use, dev->perc,
                  fs, model, mount, uuid, label

        );

        zfree(label);
        zfree(uuid);
        zfree(mount);
        zfree(model);
        zfree(fs);
        zfree(syspath);
        zfree(name);
    }

    list_iter_release(list_it);
#endif
}

typedef void(* sampled_device_cb)(list_t*);

void blkdev_sample(double sample_size_sec, sampled_device_cb cb) {
    list_t* devs_a = NULL;
    list_t* devs_b = NULL;

    blkdev_get(&devs_a);

#ifndef HW_NO_SLEEP
    nsleepd(sample_size_sec);
#endif

    blkdev_get(&devs_b);

    list_iter_t* it = NULL;
    list_iter_init(devs_a, &it);

    blk_dev_t* dev_a;
    blk_dev_t* dev_b;
    while ((dev_a = list_iter_next(it))) {
        list_iter_t* it2 = NULL;
        list_iter_init(devs_b, &it2);

        while ((dev_b = list_iter_next(it2))) {
            if (string_compare(dev_a->name, dev_b->name) == ST_OK) {
                blk_dev_diff(dev_a, dev_b, sample_size_sec);
                break;
            }

        }

        list_iter_release(it2);
    }

    list_iter_release(it);

    list_release(devs_a, true);

    if (cb)
        cb(devs_b);
}


