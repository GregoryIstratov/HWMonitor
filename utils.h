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

//============================================================================================================
// FILE UTILS
//============================================================================================================

u64 get_sfile_size(const char* filename);

typedef void(* cmd_exec_cb)(void*, list_t*);

void file_mmap_string(const char* filename, string* s);

u64 get_fd_file_size(int fd);

void fd_file_mmap(int fd, string* s);

void file_read_all(const char* filename, char** buff, u64* size);

void file_read_all_s(const char* filename, string* s);

int file_read_all_buffered_s(const char* filename, string* s);

void file_read_line(const char* filename, string* s);

string* file_read_subdir(string* subdir, const char* filepath);

enum {
    HR_SIZE_KB,
    HR_SIZE_MB,
    HR_SIZE_GB
};

void human_readable_size(u64 bytes, double* result, int* type);

//============================================================================================================
// CMD EXECUTOR
//============================================================================================================
ret_t cmd_execute(const char* cmd, void* ctx, cmd_exec_cb cb);
