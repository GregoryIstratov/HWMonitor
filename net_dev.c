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

#include <dirent.h>
#include <memory.h>
#include <math.h>
#include "net_dev.h"
#include "allocators.h"
#include "timer.h"
#include "utils.h"

void net_dev_release_cb(void* p) {
    net_dev_t* dev = (net_dev_t*)p;
    if (dev) {
        if (dev->name)
            string_release(dev->name);
        if (dev->sysdir)
            string_release(dev->sysdir);
        if (dev->mtu)
            string_release(dev->mtu);
        if (dev->speed)
            string_release(dev->speed);
        zfree(dev);
    }
}


void net_dev_scan(list_t* devs) {
    struct dirent* dir = NULL;

    DIR* d = opendir("/sys/class/net/");

    if (d) {
        while ((dir = readdir(d))) {

            if (strcmp(dir->d_name, ".") == 0)
                continue;

            if (strcmp(dir->d_name, "..") == 0)
                continue;

            string* dir_name = NULL;
            string_create(&dir_name, dir->d_name);

            net_dev_t* dev = zalloc(sizeof(net_dev_t));

            // create sysdir
            string* sysdir = NULL;
            string_init(&sysdir);
            string_append(sysdir, "/sys/class/net/");
            string_add(sysdir, dir_name);
            string_append(sysdir, "/");

            // set name and sysdir
            string_dub(dir_name, &dev->name);
            dev->sysdir = sysdir;

            // getting stats

            dev->mtu = file_read_subdir(sysdir, "mtu");
            dev->speed = file_read_subdir(sysdir, "speed");
            string* rx_bytes_s = file_read_subdir(sysdir, "statistics/rx_bytes");
            string* tx_bytes_s = file_read_subdir(sysdir, "statistics/tx_bytes");

            string_to_u64(rx_bytes_s, &dev->rx_bytes);
            string_to_u64(tx_bytes_s, &dev->tx_bytes);

            string_release(rx_bytes_s);
            string_release(tx_bytes_s);

            // add dev to list
            list_push(devs, dev);

            string_release(dir_name);
        }

        closedir(d);
    }
}

void net_dev_diff(net_dev_t* __restrict a, net_dev_t* __restrict b, double sample_rate) {
    u64 drx = b->rx_bytes - a->rx_bytes;
    u64 dtx = b->tx_bytes - a->tx_bytes;

    u64 ispeed = 0;
    string_to_u64(b->speed, &ispeed);
    ispeed /= 8; // to megabytes
    ispeed *= 1024 * 1024; // to bytes

    if (!ispeed)
        string_append(b->speed, "0 Mbits");
    else
        string_append(b->speed, " Mbits");

    double pure_rxtx = (double)(drx + dtx) / sample_rate;
    b->bandwidth_use = pure_rxtx / (double)ispeed;

    if (isnan(b->bandwidth_use))
        b->bandwidth_use = 0.0;

    b->rx_speed = drx / sample_rate;
    b->tx_speed = dtx / sample_rate;
}

//============================================================================================================
// NET DEVICE SAMPLING
//============================================================================================================

void net_dev_get(list_t** devs) {
    list_init(devs, &net_dev_release_cb);

    net_dev_scan(*devs);
}

void net_dev_sample(double sample_size_sec, sampled_device_cb cb) {
    list_t* devs_a = NULL;
    list_t* devs_b = NULL;

    net_dev_get(&devs_a);

#ifndef HW_NO_SLEEP
    nsleepd(sample_size_sec);
#endif

    net_dev_get(&devs_b);

    list_iter_t* it = NULL;
    list_iter_init(devs_a, &it);

    net_dev_t* dev_a;
    net_dev_t* dev_b;
    while ((dev_a = list_iter_next(it))) {
        list_iter_t* it2 = NULL;
        list_iter_init(devs_b, &it2);

        while ((dev_b = list_iter_next(it2))) {
            if (string_compare(dev_a->name, dev_b->name) == ST_OK) {
                net_dev_diff(dev_a, dev_b, sample_size_sec);
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
