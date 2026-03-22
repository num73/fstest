/*
    fstest 通用定义和工具函数
    包含常量定义、时间计算、缓冲区操作、文件操作等公共功能
*/

#ifndef FSTEST_COMMON_H
#define FSTEST_COMMON_H

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef long long ll;

/* 大小常量 */
#define _1KB_BYTES (1024L)
#define _1MB_BYTES (1024 * 1024L)
#define _1GB_BYTES (1024 * 1024 * 1024L)
#define NANOS_PER_SECOND (1000000000L)

/* 默认参数 */
#define DEFAULT_IO_SIZE (4 * _1KB_BYTES)
#define DEFAULT_FILE_SIZE (256 * _1MB_BYTES)
#define DEFAULT_JOBS 1
#define DEFAULT_ITER 5
#define MAX_JOBS 64
#define MAX_PATH_LEN 512

/* 测试结果宏 */
#define TEST_PASS(name) \
    printf("  [PASS] %s\n", (name))

#define TEST_FAIL(name, reason) \
    printf("  [FAIL] %s: %s\n", (name), (reason))

#define TEST_SKIP(name, reason) \
    printf("  [SKIP] %s: %s\n", (name), (reason))

enum fstest_mode {
    TEST_MODE_ALL = 0,
    TEST_MODE_FUNCTIONAL = 1,
    TEST_MODE_CONSISTENCY = 2,
    TEST_MODE_EXCEPTION = 3,
    TEST_MODE_CONCURRENT = 4,
    TEST_MODE_STRESS = 5,
    TEST_MODE_PERFORMANCE = 6,
};

/* 全局配置结构 */
struct fstest_config {
    char dir[MAX_PATH_LEN];   /* 测试目录 */
    int jobs;                  /* 并发线程数 */
    size_t io_size;            /* IO 大小 (bytes) */
    size_t file_size;          /* 测试文件大小 (bytes) */
    int iter_count;            /* 迭代次数 */
    enum fstest_mode test_mode; /* 测试模式 (all/functional/...) */
    int verbose;               /* 详细输出 */
};

/* 性能测试线程信息 */
struct test_info {
    char *file_name;
    int fd;
    void *buf;
    size_t file_size;
    int iter_count;
    size_t io_size;
    size_t total_bytes;
};

enum test_type { SEQ_READ, SEQ_WRITE, RAND_READ, RAND_WRITE };

/* 工具函数声明 */
int64_t calculate_time_diff_ns(struct timespec *start, struct timespec *end);
void fill_rand_buffer(char *buf, size_t size);
void rm_file_if_exists(const char *path);
void make_test_path(char *out, size_t out_size, const char *dir,
                    const char *name);
int ensure_dir_exists(const char *path);
void remove_dir_recursive(const char *path);

#endif /* FSTEST_COMMON_H */
