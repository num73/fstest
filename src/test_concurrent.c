/*
    并发测试模块实现
    测试项目：
    - 并发读写同一文件
    - 并发创建/删除文件
    - 并发目录操作
    - 文件锁测试
    - 竞争条件检测
*/

#include "test_concurrent.h"

#include <sys/file.h>

/* 并发写入同一文件的线程参数 */
struct concurrent_rw_args {
    const char *path;
    int thread_id;
    size_t io_size;
    int iterations;
    int errors;
};

/* 并发创建/删除文件的线程参数 */
struct concurrent_create_args {
    const char *dir;
    int thread_id;
    int file_count;
    int errors;
};

/* 线程函数：并发写入同一文件 */
static void *concurrent_write_job(void *arg) {
    struct concurrent_rw_args *a = (struct concurrent_rw_args *)arg;
    a->errors = 0;

    char *buf = malloc(a->io_size);
    if (!buf) {
        a->errors = 1;
        return NULL;
    }
    memset(buf, 'A' + (a->thread_id % 26), a->io_size);

    for (int i = 0; i < a->iterations; i++) {
        int fd = open(a->path, O_WRONLY);
        if (fd < 0) {
            a->errors++;
            continue;
        }
        off_t offset = (off_t)(a->thread_id * a->io_size);
        lseek(fd, offset, SEEK_SET);
        ssize_t w = write(fd, buf, a->io_size);
        if (w != (ssize_t)a->io_size) {
            a->errors++;
        }
        close(fd);
    }

    free(buf);
    return NULL;
}

/* 线程函数：并发读取同一文件 */
static void *concurrent_read_job(void *arg) {
    struct concurrent_rw_args *a = (struct concurrent_rw_args *)arg;
    a->errors = 0;

    char *buf = malloc(a->io_size);
    if (!buf) {
        a->errors = 1;
        return NULL;
    }

    for (int i = 0; i < a->iterations; i++) {
        int fd = open(a->path, O_RDONLY);
        if (fd < 0) {
            a->errors++;
            continue;
        }
        off_t offset = (off_t)(a->thread_id * a->io_size);
        lseek(fd, offset, SEEK_SET);
        ssize_t r = read(fd, buf, a->io_size);
        if (r < 0) {
            a->errors++;
        }
        close(fd);
    }

    free(buf);
    return NULL;
}

/* 测试：并发读写同一文件 */
static void test_concurrent_rw(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "conc_rw_test.dat");

    int nthreads = cfg->jobs > 1 ? cfg->jobs : 4;
    if (nthreads > MAX_JOBS) nthreads = MAX_JOBS;
    size_t io_size = cfg->io_size;
    size_t file_size = nthreads * io_size;

    /* 创建初始文件 */
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("concurrent rw", strerror(errno));
        return;
    }
    char *init_buf = calloc(1, file_size);
    (void)!write(fd, init_buf, file_size);
    free(init_buf);
    close(fd);

    /* 启动写线程 */
    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));
    struct concurrent_rw_args *args =
        malloc(nthreads * sizeof(struct concurrent_rw_args));

    for (int i = 0; i < nthreads; i++) {
        args[i].path = path;
        args[i].thread_id = i;
        args[i].io_size = io_size;
        args[i].iterations = 100;
        args[i].errors = 0;
        pthread_create(&threads[i], NULL, concurrent_write_job, &args[i]);
    }
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    int total_errors = 0;
    for (int i = 0; i < nthreads; i++) {
        total_errors += args[i].errors;
    }
    if (total_errors == 0) {
        TEST_PASS("concurrent write to same file");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d errors in %d threads",
                 total_errors, nthreads);
        TEST_FAIL("concurrent write", msg);
    }

    /* 启动读线程 */
    for (int i = 0; i < nthreads; i++) {
        args[i].errors = 0;
        pthread_create(&threads[i], NULL, concurrent_read_job, &args[i]);
    }
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    total_errors = 0;
    for (int i = 0; i < nthreads; i++) {
        total_errors += args[i].errors;
    }
    if (total_errors == 0) {
        TEST_PASS("concurrent read from same file");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d errors in %d threads",
                 total_errors, nthreads);
        TEST_FAIL("concurrent read", msg);
    }

    free(threads);
    free(args);
    unlink(path);
}

/* 线程函数：并发创建/删除文件 */
static void *concurrent_create_delete_job(void *arg) {
    struct concurrent_create_args *a =
        (struct concurrent_create_args *)arg;
    a->errors = 0;

    for (int i = 0; i < a->file_count; i++) {
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/conc_cd_%d_%d.dat",
                 a->dir, a->thread_id, i);

        /* 创建文件 */
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            a->errors++;
            continue;
        }
        (void)!write(fd, "test", 4);
        close(fd);

        /* 立即删除 */
        if (unlink(path) != 0) {
            a->errors++;
        }
    }
    return NULL;
}

/* 测试：并发创建/删除文件 */
static void test_concurrent_create_delete(const struct fstest_config *cfg) {
    int nthreads = cfg->jobs > 1 ? cfg->jobs : 4;
    if (nthreads > MAX_JOBS) nthreads = MAX_JOBS;
    int files_per_thread = 100;

    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));
    struct concurrent_create_args *args =
        malloc(nthreads * sizeof(struct concurrent_create_args));

    for (int i = 0; i < nthreads; i++) {
        args[i].dir = cfg->dir;
        args[i].thread_id = i;
        args[i].file_count = files_per_thread;
        args[i].errors = 0;
        pthread_create(&threads[i], NULL, concurrent_create_delete_job,
                       &args[i]);
    }
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    int total_errors = 0;
    for (int i = 0; i < nthreads; i++) {
        total_errors += args[i].errors;
    }

    if (total_errors == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "concurrent create/delete (%d threads x %d files)",
                 nthreads, files_per_thread);
        TEST_PASS(msg);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d errors in %d threads",
                 total_errors, nthreads);
        TEST_FAIL("concurrent create/delete", msg);
    }

    free(threads);
    free(args);
}

/* 并发目录操作的线程参数 */
struct concurrent_dir_args {
    const char *base_dir;
    int thread_id;
    int dir_count;
    int errors;
};

static void *concurrent_dir_job(void *arg) {
    struct concurrent_dir_args *a = (struct concurrent_dir_args *)arg;
    a->errors = 0;

    for (int i = 0; i < a->dir_count; i++) {
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/conc_dir_%d_%d",
                 a->base_dir, a->thread_id, i);

        if (mkdir(path, 0755) != 0) {
            a->errors++;
            continue;
        }

        /* 在目录中创建一个文件 */
        char fpath[MAX_PATH_LEN];
        snprintf(fpath, sizeof(fpath), "%s/test.txt", path);
        int fd = open(fpath, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) {
            (void)!write(fd, "dir test", 8);
            close(fd);
            unlink(fpath);
        }

        if (rmdir(path) != 0) {
            a->errors++;
        }
    }
    return NULL;
}

/* 测试：并发目录操作 */
static void test_concurrent_dir_ops(const struct fstest_config *cfg) {
    int nthreads = cfg->jobs > 1 ? cfg->jobs : 4;
    if (nthreads > MAX_JOBS) nthreads = MAX_JOBS;
    int dirs_per_thread = 50;

    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));
    struct concurrent_dir_args *args =
        malloc(nthreads * sizeof(struct concurrent_dir_args));

    for (int i = 0; i < nthreads; i++) {
        args[i].base_dir = cfg->dir;
        args[i].thread_id = i;
        args[i].dir_count = dirs_per_thread;
        args[i].errors = 0;
        pthread_create(&threads[i], NULL, concurrent_dir_job, &args[i]);
    }
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    int total_errors = 0;
    for (int i = 0; i < nthreads; i++) {
        total_errors += args[i].errors;
    }

    if (total_errors == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "concurrent dir ops (%d threads x %d dirs)",
                 nthreads, dirs_per_thread);
        TEST_PASS(msg);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d errors in %d threads",
                 total_errors, nthreads);
        TEST_FAIL("concurrent dir ops", msg);
    }

    free(threads);
    free(args);
}

/* 文件锁并发测试参数 */
struct lock_test_args {
    const char *path;
    int thread_id;
    int iterations;
    int lock_errors;
    int data_errors;
};

static void *file_lock_job(void *arg) {
    struct lock_test_args *a = (struct lock_test_args *)arg;
    a->lock_errors = 0;
    a->data_errors = 0;

    for (int i = 0; i < a->iterations; i++) {
        int fd = open(a->path, O_RDWR);
        if (fd < 0) {
            a->lock_errors++;
            continue;
        }

        /* 获取排他锁 */
        if (flock(fd, LOCK_EX) != 0) {
            a->lock_errors++;
            close(fd);
            continue;
        }

        /* 读取当前计数器 */
        lseek(fd, 0, SEEK_SET);
        int counter = 0;
        if (read(fd, &counter, sizeof(counter)) != sizeof(counter)) {
            counter = 0;
        }

        /* 增加计数器 */
        counter++;
        lseek(fd, 0, SEEK_SET);
        (void)!write(fd, &counter, sizeof(counter));

        /* 释放锁 */
        flock(fd, LOCK_UN);
        close(fd);
    }
    return NULL;
}

/* 测试：文件锁保护的并发计数器 */
static void test_file_lock_counter(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "conc_lock_counter.dat");

    int nthreads = cfg->jobs > 1 ? cfg->jobs : 4;
    if (nthreads > MAX_JOBS) nthreads = MAX_JOBS;
    int iterations = 100;

    /* 初始化计数器文件 */
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("file lock counter", strerror(errno));
        return;
    }
    int zero = 0;
    (void)!write(fd, &zero, sizeof(zero));
    close(fd);

    /* 启动线程 */
    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));
    struct lock_test_args *args =
        malloc(nthreads * sizeof(struct lock_test_args));

    for (int i = 0; i < nthreads; i++) {
        args[i].path = path;
        args[i].thread_id = i;
        args[i].iterations = iterations;
        pthread_create(&threads[i], NULL, file_lock_job, &args[i]);
    }
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* 验证最终计数器值 */
    fd = open(path, O_RDONLY);
    int final_count = 0;
    (void)!read(fd, &final_count, sizeof(final_count));
    close(fd);

    int expected = nthreads * iterations;
    if (final_count == expected) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "file lock counter (%d threads x %d iter = %d)",
                 nthreads, iterations, expected);
        TEST_PASS(msg);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "expected %d, got %d (race condition detected)",
                 expected, final_count);
        TEST_FAIL("file lock counter", msg);
    }

    int total_lock_errors = 0;
    for (int i = 0; i < nthreads; i++) {
        total_lock_errors += args[i].lock_errors;
    }
    if (total_lock_errors > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d lock errors", total_lock_errors);
        TEST_FAIL("file lock reliability", msg);
    }

    free(threads);
    free(args);
    unlink(path);
}

/* 竞争条件检测：不使用锁的并发计数器 */
static void test_race_condition_detect(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "conc_race_test.dat");

    int nthreads = cfg->jobs > 1 ? cfg->jobs : 4;
    if (nthreads > MAX_JOBS) nthreads = MAX_JOBS;
    if (nthreads < 2) nthreads = 2;

    /* 创建共享文件 */
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("race condition detect", strerror(errno));
        return;
    }
    /* 写入大量数据，每线程写不同模式 */
    size_t chunk = 4096;
    size_t total = chunk * nthreads;
    char *init = calloc(1, total);
    (void)!write(fd, init, total);
    free(init);
    close(fd);

    /* 多线程同时写入不同区域（应该不冲突） */
    struct concurrent_rw_args *args =
        malloc(nthreads * sizeof(struct concurrent_rw_args));
    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));

    for (int i = 0; i < nthreads; i++) {
        args[i].path = path;
        args[i].thread_id = i;
        args[i].io_size = chunk;
        args[i].iterations = 50;
        args[i].errors = 0;
        pthread_create(&threads[i], NULL, concurrent_write_job, &args[i]);
    }
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* 验证每个区域的数据 */
    fd = open(path, O_RDONLY);
    char *rbuf = malloc(chunk);
    int pass = 1;
    for (int i = 0; i < nthreads; i++) {
        lseek(fd, i * chunk, SEEK_SET);
        (void)!read(fd, rbuf, chunk);
        char expected = 'A' + (i % 26);
        for (size_t j = 0; j < chunk; j++) {
            if (rbuf[j] != expected) {
                pass = 0;
                break;
            }
        }
        if (!pass) break;
    }
    close(fd);
    free(rbuf);

    if (pass) {
        TEST_PASS("race condition detect (isolated regions OK)");
    } else {
        TEST_FAIL("race condition detect",
                  "data corruption in isolated regions");
    }

    free(args);
    free(threads);
    unlink(path);
}

void run_concurrent_tests(const struct fstest_config *cfg) {
    printf("\n");
    printf("========================================\n");
    printf("  4. 并发测试 (Concurrency Tests)\n");
    printf("========================================\n");
    printf("  Threads: %d\n", cfg->jobs > 1 ? cfg->jobs : 4);

    test_concurrent_rw(cfg);
    test_concurrent_create_delete(cfg);
    test_concurrent_dir_ops(cfg);
    test_file_lock_counter(cfg);
    test_race_condition_detect(cfg);

    printf("--- 并发测试完成 ---\n");
}
