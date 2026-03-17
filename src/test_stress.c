/*
    压力和稳定性测试模块实现
    测试项目：
    - 海量小文件
    - 超大文件
    - 深层目录结构
    - 高频创建/删除/重命名
    - 循环读写测试
*/

#include "test_stress.h"

#include <sys/stat.h>

/* 测试：海量小文件 */
static void test_mass_small_files(const struct fstest_config *cfg) {
    char subdir[MAX_PATH_LEN];
    make_test_path(subdir, sizeof(subdir), cfg->dir, "stress_small_files");
    mkdir(subdir, 0755);

    int file_count = 1000;
    struct timespec start, end;

    /* 创建大量小文件 */
    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    int created = 0;
    for (int i = 0; i < file_count; i++) {
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/file_%05d.dat", subdir, i);
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) break;
        char data[64];
        snprintf(data, sizeof(data), "small file %d", i);
        (void)!write(fd, data, strlen(data));
        close(fd);
        created++;
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    double duration_ms =
        calculate_time_diff_ns(&start, &end) / 1000000.0;

    if (created == file_count) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "mass small files: %d files in %.1f ms (%.0f files/s)",
                 file_count, duration_ms,
                 file_count / (duration_ms / 1000.0));
        TEST_PASS(msg);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "only created %d/%d files", created, file_count);
        TEST_FAIL("mass small files", msg);
    }

    /* 删除所有文件 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < created; i++) {
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/file_%05d.dat", subdir, i);
        unlink(path);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    duration_ms = calculate_time_diff_ns(&start, &end) / 1000000.0;

    printf("  [INFO] deleted %d files in %.1f ms (%.0f files/s)\n",
           created, duration_ms,
           created / (duration_ms / 1000.0));

    rmdir(subdir);
}

/* 测试：超大文件 */
static void test_large_file(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "stress_large.dat");

    /* 使用配置的 file_size */
    size_t target_size = cfg->file_size;
    size_t block_size = 4 * _1MB_BYTES;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("large file write", strerror(errno));
        return;
    }

    char *buf = malloc(block_size);
    if (!buf) {
        TEST_FAIL("large file write", "malloc failed");
        close(fd);
        return;
    }
    fill_rand_buffer(buf, block_size);

    size_t written = 0;
    while (written < target_size) {
        size_t to_write = block_size;
        if (written + to_write > target_size) {
            to_write = target_size - written;
        }
        ssize_t w = write(fd, buf, to_write);
        if (w <= 0) {
            break;
        }
        written += w;
    }
    close(fd);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration_s =
        calculate_time_diff_ns(&start, &end) / (double)NANOS_PER_SECOND;
    double throughput = (written / (double)_1MB_BYTES) / duration_s;

    if (written == target_size) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "large file write: %zu MB in %.2f s (%.2f MB/s)",
                 target_size / _1MB_BYTES, duration_s, throughput);
        TEST_PASS(msg);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "wrote %zu/%zu bytes", written, target_size);
        TEST_FAIL("large file write", msg);
    }

    /* 读回验证 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    fd = open(path, O_RDONLY);
    size_t total_read = 0;
    while (total_read < written) {
        ssize_t r = read(fd, buf, block_size);
        if (r <= 0) break;
        total_read += r;
    }
    close(fd);
    clock_gettime(CLOCK_MONOTONIC, &end);

    duration_s =
        calculate_time_diff_ns(&start, &end) / (double)NANOS_PER_SECOND;
    throughput = (total_read / (double)_1MB_BYTES) / duration_s;

    if (total_read == written) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "large file read: %zu MB in %.2f s (%.2f MB/s)",
                 total_read / _1MB_BYTES, duration_s, throughput);
        TEST_PASS(msg);
    } else {
        TEST_FAIL("large file read", "read size mismatch");
    }

    free(buf);
    unlink(path);
}

/* 测试：深层目录结构 */
static void test_deep_directory(const struct fstest_config *cfg) {
    int depth = 50;
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "stress_deep");

    /* 逐级创建目录 */
    char current[MAX_PATH_LEN];
    strncpy(current, path, sizeof(current) - 1);
    current[sizeof(current) - 1] = '\0';

    int created_depth = 0;
    for (int i = 0; i < depth; i++) {
        if (mkdir(current, 0755) != 0 && errno != EEXIST) {
            break;
        }
        created_depth++;

        size_t len = strlen(current);
        if (len + 3 >= sizeof(current)) break;
        snprintf(current + len, sizeof(current) - len, "/d");
    }

    /* 在最深层创建文件 */
    char fpath[MAX_PATH_LEN];
    snprintf(fpath, sizeof(fpath), "%s/deep_file.txt", current);
    int fd = open(fpath, O_CREAT | O_WRONLY, 0644);
    int file_ok = 0;
    if (fd >= 0) {
        (void)!write(fd, "deep!", 5);
        close(fd);

        /* 读回验证 */
        fd = open(fpath, O_RDONLY);
        if (fd >= 0) {
            char buf[16] = {0};
            (void)!read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (strcmp(buf, "deep!") == 0) {
                file_ok = 1;
            }
        }
        unlink(fpath);
    }

    if (created_depth == depth && file_ok) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "deep directory: %d levels, file r/w OK", depth);
        TEST_PASS(msg);
    } else if (created_depth > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "created %d/%d levels, file_ok=%d",
                 created_depth, depth, file_ok);
        TEST_PASS(msg);
    } else {
        TEST_FAIL("deep directory", "could not create directories");
    }

    /* 递归删除 */
    remove_dir_recursive(path);
}

/* 测试：高频创建/删除/重命名 */
static void test_high_freq_metadata(const struct fstest_config *cfg) {
    char subdir[MAX_PATH_LEN];
    make_test_path(subdir, sizeof(subdir), cfg->dir, "stress_freq");
    mkdir(subdir, 0755);

    int ops = 500;
    struct timespec start, end;
    int errors = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ops; i++) {
        char path1[MAX_PATH_LEN], path2[MAX_PATH_LEN];
        snprintf(path1, sizeof(path1), "%s/freq_%d.dat", subdir, i);
        snprintf(path2, sizeof(path2), "%s/freq_%d_renamed.dat",
                 subdir, i);

        /* 创建 */
        int fd = open(path1, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            errors++;
            continue;
        }
        (void)!write(fd, "x", 1);
        close(fd);

        /* 重命名 */
        if (rename(path1, path2) != 0) {
            errors++;
            unlink(path1);
            continue;
        }

        /* 删除 */
        if (unlink(path2) != 0) {
            errors++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration_ms =
        calculate_time_diff_ns(&start, &end) / 1000000.0;
    double ops_per_sec =
        (ops * 3.0) / (duration_ms / 1000.0); /* 3 ops per iteration */

    if (errors == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "high freq metadata: %d cycles in %.1f ms "
                 "(%.0f ops/s)",
                 ops, duration_ms, ops_per_sec);
        TEST_PASS(msg);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d errors in %d cycles",
                 errors, ops);
        TEST_FAIL("high freq metadata", msg);
    }

    rmdir(subdir);
}

/* 测试：循环读写 */
static void test_loop_rw(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "stress_loop.dat");

    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("loop rw", strerror(errno));
        return;
    }

    size_t io_size = cfg->io_size;
    char *wbuf = malloc(io_size);
    char *rbuf = malloc(io_size);
    if (!wbuf || !rbuf) {
        TEST_FAIL("loop rw", "malloc failed");
        free(wbuf);
        free(rbuf);
        close(fd);
        return;
    }

    int loops = cfg->iter_count * 100;
    int errors = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < loops; i++) {
        srand(i);
        fill_rand_buffer(wbuf, io_size);

        lseek(fd, 0, SEEK_SET);
        if (write(fd, wbuf, io_size) != (ssize_t)io_size) {
            errors++;
            continue;
        }

        lseek(fd, 0, SEEK_SET);
        if (read(fd, rbuf, io_size) != (ssize_t)io_size) {
            errors++;
            continue;
        }

        if (memcmp(wbuf, rbuf, io_size) != 0) {
            errors++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration_s =
        calculate_time_diff_ns(&start, &end) / (double)NANOS_PER_SECOND;

    close(fd);

    if (errors == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "loop rw: %d cycles in %.2f s, all verified OK",
                 loops, duration_s);
        TEST_PASS(msg);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d/%d errors", errors, loops);
        TEST_FAIL("loop rw", msg);
    }

    free(wbuf);
    free(rbuf);
    unlink(path);
}

void run_stress_tests(const struct fstest_config *cfg) {
    printf("\n");
    printf("========================================\n");
    printf("  5. 压力稳定性测试 (Stress Tests)\n");
    printf("========================================\n");

    test_mass_small_files(cfg);
    test_large_file(cfg);
    test_deep_directory(cfg);
    test_high_freq_metadata(cfg);
    test_loop_rw(cfg);

    printf("--- 压力稳定性测试完成 ---\n");
}
