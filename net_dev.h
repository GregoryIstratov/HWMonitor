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

#include "string.h"

//============================================================================================================
// NET DEVICE
//============================================================================================================

typedef struct net_dev {
    string* name;
    string* sysdir;
    u64 rx_bytes;
    u64 tx_bytes;
    double tx_speed;
    double rx_speed;
    string* speed;
    string* mtu;
    double bandwidth_use;
} net_dev_t;

void net_dev_release_cb(void* p);

void net_dev_scan(list_t* devs);

void net_dev_diff(net_dev_t* __restrict a, net_dev_t* __restrict b, double sample_rate);

//============================================================================================================
// NET DEVICE SAMPLING
//============================================================================================================

void net_dev_get(list_t** devs);

typedef void(* sampled_device_cb)(list_t*);

void net_dev_sample(double sample_size_sec, sampled_device_cb cb);
