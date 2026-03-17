/*
    性能测试模块实现
    整合自 performance/ 目录下的各个测试，统一为可配置的性能测试
    测试项目：
    - 顺序读写吞吐
    - 随机读写 IOPS
    - 延迟统计
    - 元数据操作性能 (create/stat/rename/unlink)
    - 不同块大小、不同并发数下的表现
*/

#include "test_performance.h"

#include <sys/stat.h>

/* 文件名管理 */
static char **perf_filenames = NULL;
static int perf_filenames_count = 0;

static void init_perf_filenames(const char *dir, int n) {
    perf_filenames = malloc(n * sizeof(char *));
    perf_filenames_count = n;
    for (int i = 0; i < n; i++) {
        perf_filenames[i] = malloc(MAX_PATH_LEN);
        snprintf(perf_filenames[i], MAX_PATH_LEN,
                 "%s/perf_testfile_%d.dat", dir, i);
    }
}

static void free_perf_filenames(void) {
    if (perf_filenames) {
        for (int i = 0; i < perf_filenames_count; i++) {
            free(perf_filenames[i]);
        }
        free(perf_filenames);
        perf_filenames = NULL;
        perf_filenames_count = 0;
    }
}

static void create_perf_file(const char *filename, size_t file_size) {
    size_t block_size = 4 * _1MB_BYTES;
    size_t block_count = file_size / block_size;

    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("create_perf_file");
        return;
    }
    char *buf = malloc(block_size);
    fill_rand_buffer(buf, block_size);

    for (size_t i = 0; i < block_count; i++) {
        if (write(fd, buf, block_size) != (ssize_t)block_size) {
            break;
        }
    }
    /* 写入余量 */
    size_t remainder = file_size % block_size;
    if (remainder > 0) {
        (void)!write(fd, buf, remainder);
    }
    close(fd);
    free(buf);
}

/* 性能测试线程函数 */
static void *perf_seq_read_job(void *arg) {
    struct test_info *info = (struct test_info *)arg;
    size_t block_count = info->file_size / info->io_size;
    size_t total_bytes = 0;

    for (int i = 0; i < info->iter_count; i++) {
        for (size_t j = 0; j < block_count; j++) {
            ssize_t r = read(info->fd, info->buf, info->io_size);
            if (r <= 0) break;
            total_bytes += r;
        }
        lseek(info->fd, 0, SEEK_SET);
    }
    info->total_bytes = total_bytes;
    return NULL;
}

static void *perf_seq_write_job(void *arg) {
    struct test_info *info = (struct test_info *)arg;
    size_t block_count = info->file_size / info->io_size;
    size_t total_bytes = 0;

    for (int i = 0; i < info->iter_count; i++) {
        for (size_t j = 0; j < block_count; j++) {
            ssize_t w = write(info->fd, info->buf, info->io_size);
            if (w <= 0) break;
            total_bytes += w;
        }
        lseek(info->fd, 0, SEEK_SET);
    }
    info->total_bytes = total_bytes;
    return NULL;
}

static void *perf_rand_read_job(void *arg) {
    struct test_info *info = (struct test_info *)arg;
    size_t block_count = info->file_size / info->io_size;
    size_t total_bytes = 0;
    unsigned int seed = (unsigned int)(uintptr_t)info;

    for (int i = 0; i < info->iter_count; i++) {
        for (size_t j = 0; j < block_count; j++) {
            off_t offset =
                (off_t)(rand_r(&seed) % block_count) * info->io_size;
            lseek(info->fd, offset, SEEK_SET);
            ssize_t r = read(info->fd, info->buf, info->io_size);
            if (r <= 0) break;
            total_bytes += r;
        }
    }
    info->total_bytes = total_bytes;
    return NULL;
}

static void *perf_rand_write_job(void *arg) {
    struct test_info *info = (struct test_info *)arg;
    size_t block_count = info->file_size / info->io_size;
    size_t total_bytes = 0;
    unsigned int seed = (unsigned int)(uintptr_t)info;

    for (int i = 0; i < info->iter_count; i++) {
        for (size_t j = 0; j < block_count; j++) {
            off_t offset =
                (off_t)(rand_r(&seed) % block_count) * info->io_size;
            lseek(info->fd, offset, SEEK_SET);
            ssize_t w = write(info->fd, info->buf, info->io_size);
            if (w <= 0) break;
            total_bytes += w;
        }
    }
    info->total_bytes = total_bytes;
    return NULL;
}

/* 多线程性能测试 */
static double run_perf_test(int job_n, size_t io_size, size_t file_size,
                            int iter_count, enum test_type type) {
    void *(*test_job)(void *) = NULL;
    const char *type_str;
    int open_flags;
    switch (type) {
        case SEQ_READ:
            test_job = perf_seq_read_job;
            type_str = "Sequential Read";
            open_flags = O_RDONLY;
            break;
        case SEQ_WRITE:
            test_job = perf_seq_write_job;
            type_str = "Sequential Write";
            open_flags = O_WRONLY | O_CREAT;
            break;
        case RAND_READ:
            test_job = perf_rand_read_job;
            type_str = "Random Read";
            open_flags = O_RDONLY;
            break;
        case RAND_WRITE:
            test_job = perf_rand_write_job;
            type_str = "Random Write";
            open_flags = O_WRONLY | O_CREAT;
            break;
    }

    struct test_info *infos = malloc(job_n * sizeof(struct test_info));
    for (int i = 0; i < job_n; i++) {
        infos[i].file_name = perf_filenames[i];
        infos[i].fd = open(perf_filenames[i], open_flags, 0644);
        if (infos[i].fd < 0) {
            printf("  [ERROR] Cannot open %s: %s\n",
                   perf_filenames[i], strerror(errno));
            for (int j = 0; j < i; j++) {
                close(infos[j].fd);
                free(infos[j].buf);
            }
            free(infos);
            return 0.0;
        }
        (void)!posix_memalign(&infos[i].buf, io_size, io_size);
        fill_rand_buffer(infos[i].buf, io_size);
        infos[i].file_size = file_size;
        infos[i].io_size = io_size;
        infos[i].total_bytes = 0;
        infos[i].iter_count = iter_count;
    }

    pthread_t *threads = malloc(job_n * sizeof(pthread_t));
    struct timespec start, end;
    size_t total_bytes = 0;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (int i = 0; i < job_n; i++) {
        pthread_create(&threads[i], NULL, test_job, &infos[i]);
    }
    for (int i = 0; i < job_n; i++) {
        pthread_join(threads[i], NULL);
        total_bytes += infos[i].total_bytes;
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    double duration_ns = calculate_time_diff_ns(&start, &end);
    double duration_s = duration_ns / (double)NANOS_PER_SECOND;
    double throughput_mbs =
        (total_bytes / (1024.0 * 1024.0)) / duration_s;

    for (int i = 0; i < job_n; i++) {
        close(infos[i].fd);
        free(infos[i].buf);
    }
    free(infos);
    free(threads);

    printf("  %-20s | IO: %6zuB | %2d jobs | %.2f MB/s | %.3f s\n",
           type_str, io_size, job_n, throughput_mbs, duration_s);
    return throughput_mbs;
}

/* 测试：延迟统计 (单线程单次操作延迟) */
static void test_latency(const struct fstest_config *cfg) {
    printf("\n  --- 延迟统计 (Latency) ---\n");
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "perf_latency.dat");

    /* 创建测试文件 */
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("latency test", strerror(errno));
        return;
    }
    size_t io_size = cfg->io_size;
    char *buf = malloc(io_size);
    fill_rand_buffer(buf, io_size);

    /* 先写入足够数据 */
    for (int i = 0; i < 100; i++) {
        (void)!write(fd, buf, io_size);
    }

    /* 测量写延迟 */
    int samples = 1000;
    double *latencies = malloc(samples * sizeof(double));
    struct timespec ts1, ts2;

    lseek(fd, 0, SEEK_SET);
    for (int i = 0; i < samples; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts1);
        (void)!write(fd, buf, io_size);
        clock_gettime(CLOCK_MONOTONIC, &ts2);
        latencies[i] = calculate_time_diff_ns(&ts1, &ts2) / 1000.0;
        lseek(fd, 0, SEEK_SET);
    }

    /* 计算统计量 */
    double min_lat = latencies[0], max_lat = latencies[0], sum = 0;
    for (int i = 0; i < samples; i++) {
        if (latencies[i] < min_lat) min_lat = latencies[i];
        if (latencies[i] > max_lat) max_lat = latencies[i];
        sum += latencies[i];
    }
    double avg_lat = sum / samples;
    printf("  Write latency (%zuB): avg=%.1f us, min=%.1f us, "
           "max=%.1f us\n",
           io_size, avg_lat, min_lat, max_lat);

    /* 测量读延迟 */
    lseek(fd, 0, SEEK_SET);
    for (int i = 0; i < samples; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts1);
        (void)!read(fd, buf, io_size);
        clock_gettime(CLOCK_MONOTONIC, &ts2);
        latencies[i] = calculate_time_diff_ns(&ts1, &ts2) / 1000.0;
        lseek(fd, 0, SEEK_SET);
    }

    min_lat = latencies[0];
    max_lat = latencies[0];
    sum = 0;
    for (int i = 0; i < samples; i++) {
        if (latencies[i] < min_lat) min_lat = latencies[i];
        if (latencies[i] > max_lat) max_lat = latencies[i];
        sum += latencies[i];
    }
    avg_lat = sum / samples;
    printf("  Read latency  (%zuB): avg=%.1f us, min=%.1f us, "
           "max=%.1f us\n",
           io_size, avg_lat, min_lat, max_lat);

    free(latencies);
    free(buf);
    close(fd);
    unlink(path);
}

/* 测试：元数据操作性能 */
static void test_metadata_perf(const struct fstest_config *cfg) {
    printf("\n  --- 元数据操作性能 (Metadata) ---\n");
    char subdir[MAX_PATH_LEN];
    make_test_path(subdir, sizeof(subdir), cfg->dir, "perf_meta");
    mkdir(subdir, 0755);

    int ops = 1000;
    struct timespec start, end;

    /* create 性能 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ops; i++) {
        char p[MAX_PATH_LEN];
        snprintf(p, sizeof(p), "%s/meta_%d.dat", subdir, i);
        int fd = open(p, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) close(fd);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double create_us =
        calculate_time_diff_ns(&start, &end) / 1000.0 / ops;
    printf("  create:  %.1f us/op (%d ops)\n", create_us, ops);

    /* stat 性能 */
    struct stat st;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ops; i++) {
        char p[MAX_PATH_LEN];
        snprintf(p, sizeof(p), "%s/meta_%d.dat", subdir, i);
        stat(p, &st);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double stat_us =
        calculate_time_diff_ns(&start, &end) / 1000.0 / ops;
    printf("  stat:    %.1f us/op (%d ops)\n", stat_us, ops);

    /* rename 性能 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ops; i++) {
        char p1[MAX_PATH_LEN], p2[MAX_PATH_LEN];
        snprintf(p1, sizeof(p1), "%s/meta_%d.dat", subdir, i);
        snprintf(p2, sizeof(p2), "%s/meta_%d_r.dat", subdir, i);
        rename(p1, p2);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double rename_us =
        calculate_time_diff_ns(&start, &end) / 1000.0 / ops;
    printf("  rename:  %.1f us/op (%d ops)\n", rename_us, ops);

    /* unlink 性能 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ops; i++) {
        char p[MAX_PATH_LEN];
        snprintf(p, sizeof(p), "%s/meta_%d_r.dat", subdir, i);
        unlink(p);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double unlink_us =
        calculate_time_diff_ns(&start, &end) / 1000.0 / ops;
    printf("  unlink:  %.1f us/op (%d ops)\n", unlink_us, ops);

    rmdir(subdir);
}

void run_performance_tests(const struct fstest_config *cfg) {
    printf("\n");
    printf("========================================\n");
    printf("  6. 性能测试 (Performance Tests)\n");
    printf("========================================\n");
    printf("  Directory:  %s\n", cfg->dir);
    printf("  Threads:    %d\n", cfg->jobs);
    printf("  IO Size:    %zu bytes\n", cfg->io_size);
    printf("  File Size:  %zu MB\n", cfg->file_size / _1MB_BYTES);
    printf("  Iterations: %d\n", cfg->iter_count);

    int job_n = cfg->jobs;
    if (job_n > MAX_JOBS) job_n = MAX_JOBS;

    init_perf_filenames(cfg->dir, job_n);

    /* 创建测试文件 */
    printf("\n  Creating %d test files (%zu MB each)...\n",
           job_n, cfg->file_size / _1MB_BYTES);
    for (int i = 0; i < job_n; i++) {
        create_perf_file(perf_filenames[i], cfg->file_size);
    }
    printf("  Test files created.\n");

    /* 吞吐测试 */
    printf("\n  --- 吞吐测试 (Throughput) ---\n");
    run_perf_test(job_n, cfg->io_size, cfg->file_size,
                  cfg->iter_count, SEQ_READ);
    run_perf_test(job_n, cfg->io_size, cfg->file_size,
                  cfg->iter_count, SEQ_WRITE);
    run_perf_test(job_n, cfg->io_size, cfg->file_size,
                  cfg->iter_count, RAND_READ);
    run_perf_test(job_n, cfg->io_size, cfg->file_size,
                  cfg->iter_count, RAND_WRITE);

    /* 不同块大小的测试 */
    printf("\n  --- 不同 IO 大小 (Variable IO Size) ---\n");
    size_t io_sizes[] = {_1KB_BYTES, 4 * _1KB_BYTES,
                         16 * _1KB_BYTES, 64 * _1KB_BYTES};
    int num_sizes = sizeof(io_sizes) / sizeof(io_sizes[0]);
    for (int s = 0; s < num_sizes; s++) {
        run_perf_test(job_n, io_sizes[s], cfg->file_size,
                      cfg->iter_count, SEQ_READ);
    }

    /* 延迟测试 */
    test_latency(cfg);

    /* 元数据性能测试 */
    test_metadata_perf(cfg);

    /* 清理测试文件 */
    printf("\n  Cleaning up test files...\n");
    for (int i = 0; i < job_n; i++) {
        unlink(perf_filenames[i]);
    }
    free_perf_filenames();

    printf("--- 性能测试完成 ---\n");
}
