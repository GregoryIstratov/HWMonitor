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

#include "globals.h"
#include "string.h"
#include "double_linked_list.h"
#include "vector.h"

enum {
    /// These values increment when an I/O request completes.
    READ_IO = 0UL, ///requests - number of read I/Os processed

    /// These values increment when an I/O request is merged with an already-queued I/O request.
    READ_MERGE = 1UL, /// requests - number of read I/Os merged with in-queue I/O

    /// These values count the number of sectors read from or written to this block device.
    /// The "sectors" in question are the standard UNIX 512-byte sectors, not any device- or
    /// filesystem-specific block size.
    /// The counters are incremented when the I/O completes.
    READ_SECTORS = 2UL, /// requests - number of read I/Os merged with in-queue I/O


    /// These values count the number of milliseconds that I/O requests have
    /// waited on this block device.  If there are multiple I/O requests waiting,
    /// these values will increase at a rate greater than 1000/second; for
    /// example, if 60 read requests wait for an average of 30 ms, the read_ticks
    /// field will increase by 60*30 = 1800.
    READ_TICKS = 3UL, ///milliseconds - total wait time for read requests

    /// These values increment when an I/O request completes.
    WRITE_IO = 4UL, /// requests - number of write I/Os processed

    /// These values increment when an I/O request is merged with an already-queued I/O request.
    WRITE_MERGES = 5UL, /// requests - number of write I/Os merged with in-queue I/O

    /// These values count the number of sectors read from or written to this block device.
    /// The "sectors" in question are the standard UNIX 512-byte sectors, not any device- or
    /// filesystem-specific block size.
    /// The counters are incremented when the I/O completes.
    WRITE_SECTORS = 6UL, /// sectors - number of sectors written


    /// These values count the number of milliseconds that I/O requests have
    /// waited on this block device.  If there are multiple I/O requests waiting,
    /// these values will increase at a rate greater than 1000/second; for
    /// example, if 60 read requests wait for an average of 30 ms, the read_ticks
    /// field will increase by 60*30 = 1800.
    WRITE_TICKS = 7UL, /// milliseconds - total wait time for write requests

    /// This value counts the number of I/O requests that have been issued to
    /// the device driver but have not yet completed.  It does not include I/O
    /// requests that are in the queue but not yet issued to the device driver.
    IN_FLIGHT = 8UL, /// requests - number of I/Os currently in flight

    /// This value counts the number of milliseconds during which the device has
    /// had I/O requests queued.
    IO_TICKS = 9UL, /// milliseconds - total time this block device has been active

    /// This value counts the number of milliseconds that I/O requests have waited
    /// on this block device.  If there are multiple I/O requests waiting, this
    /// value will increase as the product of the number of milliseconds times the
    /// number of requests waiting (see "read ticks" above for an example).
    TIME_IN_QUEUE = 10UL /// milliseconds - total wait time for all requests
};



//============================================================================================================
// BLOCK DEVICE MANAGEMENT
//============================================================================================================

typedef struct blk_dev {
    string* name;
    //struct statvfs stats;
    u64 stat[11];
    double perf_read;
    double perf_write;
    string* label;
    u64 size;
    u64 used;
    u64 avail;
    u64 use;
    double perc;
    string* fs;
    string* mount;
    string* sysfolder;
    string* model;
    string* uuid;
    string* shed;
} blk_dev_t;

void blk_dev_release_cb(void* p);

blk_dev_t* blk_dev_list_search(list_t* devs, string* name);

blk_dev_t* blk_dev_list_direct_search(list_t* devs, string* name);

void blk_dev_diff(blk_dev_t* __restrict a, blk_dev_t* __restrict b, double sample_size);


//============================================================================================================
// DF UTILS
//============================================================================================================
enum {
    DFS_NAME = 0,
    DFS_TOTAL = 1,
    DFS_USED = 2,
    DFS_AVAIL = 3,
    DFS_USE = 4

};

typedef struct df_stat {
    string* dev;

    u64 total;
    u64 used;
    u64 avail;
    u64 use;

} df_stat_t;

typedef struct {
    list_t* devs;
    u64 skip_first;
} df_t;

void df_init(list_t* devs, df_t** df);

void df_callback(void* ctx, list_t* lines);

void df_execute(df_t* dfs);

//============================================================================================================
// BLK UTILS
//============================================================================================================

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
    list_t* devs;
} sblkid_t;

ret_t vector_add_kv(vector_t* vec, string* key, string* val);

void sblk_callback(void* ctx, list_t* lines);

ret_t sblk_execute(sblkid_t* sblk);

//============================================================================================================
// BLOCK DEVICE SCANNER
//============================================================================================================

void blk_dev_scan(string* basedir, list_t* devs);

//============================================================================================================
// BLOCK DEVICE SAMPLING
//============================================================================================================

void blkdev_get(list_t** devs);

typedef void(* sampled_device_cb)(list_t*);

void blkdev_sample(double sample_size_sec, sampled_device_cb cb);
