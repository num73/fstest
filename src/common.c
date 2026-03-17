/*
    fstest 通用工具函数实现
*/

#include "common.h"

#include <dirent.h>
#include <sys/stat.h>

int64_t calculate_time_diff_ns(struct timespec *start, struct timespec *end) {
    int64_t seconds = end->tv_sec - start->tv_sec;
    int64_t nanoseconds = end->tv_nsec - start->tv_nsec;
    return seconds * NANOS_PER_SECOND + nanoseconds;
}

void fill_rand_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        buf[i] = rand() % 256;
    }
}

void rm_file_if_exists(const char *path) {
    if (access(path, F_OK) == 0) {
        if (remove(path) != 0) {
            /* 非致命错误，仅打印警告 */
            perror("remove existing file");
        }
    }
}

void make_test_path(char *out, size_t out_size, const char *dir,
                    const char *name) {
    snprintf(out, out_size, "%s/%s", dir, name);
}

int ensure_dir_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir(path, 0755);
}

void remove_dir_recursive(const char *path) {
    DIR *d = opendir(path);
    if (!d) return;

    struct dirent *entry;
    char full[MAX_PATH_LEN];
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        struct stat st;
        if (lstat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                remove_dir_recursive(full);
            } else {
                unlink(full);
            }
        }
    }
    closedir(d);
    rmdir(path);
}
