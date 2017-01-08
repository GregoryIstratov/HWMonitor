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

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utils.h"
#include "allocators.h"

//============================================================================================================
// FILE UTILS
//============================================================================================================

u64 get_sfile_size(const char* filename) {
    struct stat st;
    stat(filename, &st);
    return (u64)st.st_size;
}

typedef void(* cmd_exec_cb)(void*, list_t*);

void file_mmap_string(const char* filename, string* s) {
    u64 filesize = get_sfile_size(filename);
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

u64 get_fd_file_size(int fd) {
    struct stat st;
    fstat(fd, &st);
    return (u64)st.st_size;
}

void fd_file_mmap(int fd, string* s) {
    u64 filesize = get_fd_file_size(fd);

    //Execute mmap
    void* data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);


    //Write the mmapped data
    string_init(&s);
    string_appendn(s, (char*)data, filesize);

    //Cleanup
    munmap(data, filesize);
    close(fd);
}

void file_read_all(const char* filename, char** buff, u64* size) {
    FILE* f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    u64 fsize = (u64)ftell(f);
    fseek(f, 0, SEEK_SET);

    char* string = malloc(fsize);
    fread(string, fsize, 1, f);
    fclose(f);

    *buff = string;
    *size = fsize;
}

void file_read_all_s(const char* filename, string* s) {
    FILE* f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    u64 fsize = (u64)ftell(f);
    fseek(f, 0, SEEK_SET);

    da_realloc(_da(s), fsize);
    fread(_da(s)->ptr, fsize, 1, f);
    _da(s)->used = fsize;
    fclose(f);
}

int file_read_all_buffered_s(const char* filename, string* s) {
#define FILE_READ_ALL_BUFF_SIZE 1024
    FILE* f = fopen(filename, "rb");
    if (!f)
        return -1;

    char buff[FILE_READ_ALL_BUFF_SIZE] = {0};
    da_realloc(_da(s), FILE_READ_ALL_BUFF_SIZE);

    while (fgets(buff, FILE_READ_ALL_BUFF_SIZE, f))
        string_append(s, buff);

    fclose(f);

    return 0;
#undef FILE_READ_ALL_BUFF_SIZE
}

void file_read_line(const char* filename, string* s) {
    FILE* f = fopen(filename, "rb");

    char* buff = NULL;
    u64 size = 0;
    getline(&buff, &size, f);

    string_append(s, buff);

    free(buff);
    fclose(f);
}

string* file_read_subdir(string* subdir, const char* filepath) {
    string* data = NULL;
    string_init(&data);

    string* filename = NULL;
    string_dub(subdir, &filename);
    string_append(filename, filepath);

    char* filename_c = string_makez(filename);
    file_read_all_s(filename_c, data);
    string_strip(data);

    zfree(filename_c);
    string_release(filename);

    return data;
}

void human_readable_size(u64 bytes, double* result, int* type) {

    double r = bytes / 1024.;
    if (r < 1024)  // KB / sec
    {
        *result = r;
        *type = HR_SIZE_KB;
        return;
    }

    r = bytes / 1024 / 1024;
    if (r < 1024)  // MiB / sec
    {
        *result = r;
        *type = HR_SIZE_MB;
        return;
    }

    r = bytes / 1024 / 1024 / 1024;
    {
        *result = r;
        *type = HR_SIZE_GB;
        return;
    }
}

//============================================================================================================
// CMD EXECUTOR
//============================================================================================================
ret_t cmd_execute(const char* cmd, void* ctx, cmd_exec_cb cb) {
    FILE* fpipe;

    if (!(fpipe = popen(cmd, "r")))
        return ST_ERR;

    char line[1024] = {0};

    list_t* sl;
    list_init(&sl, &string_release_cb);

    while (fgets(line, sizeof(line), fpipe)) {
        string* s = NULL;

        string_create(&s, line);
        list_push(sl, s);
    }

    cb(ctx, sl);

    pclose(fpipe);

    return ST_OK;
}
