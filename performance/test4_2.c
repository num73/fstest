/*
    基础性能测试，包括顺序读、顺序写、随机读、随机写等场景
    测试函数使用posix的read，write

    不同iosize 和 线程数对性能的影响测试。
*/

#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef long long ll;
#define _1KB_BYTES (1024L)
#define _1MB_BYTES (1024 * 1024L)

#define _1GB_BYTES (1024 * 1024 * 1024L)

#define NANOS_PER_SECOND (1000000000L)

/*----全局配置------*/

#define FILE_PATH_BASE "/mnt/nufs/fs_testfile"
#define FILE_SIZE (256 * _1MB_BYTES)

char *log_filename = "performance_log.csv";
FILE *log_fp;

/*-----------------*/

char **filenames;
enum test_type { SEQ_READ, SEQ_WRITE, RAND_READ, RAND_WRITE };

struct test_info {
    char *file_name;
    int fd;
    void *buf;
    size_t file_size;
    // 正常情况下，读写的总字节数应该是 file_size * iter_count
    int iter_count;
    size_t io_size;

    // 作为返回值，实际读写的总字节数
    size_t total_bytes;
};

// 计算是时间差，单位ns
int64_t calculate_time_diff_ns(struct timespec *start, struct timespec *end) {
    int64_t seconds = end->tv_sec - start->tv_sec;
    int64_t nanoseconds = end->tv_nsec - start->tv_nsec;
    return seconds * NANOS_PER_SECOND + nanoseconds;
}

void init_filenames(int n) {
    filenames = malloc(n * sizeof(char *));
    for (int i = 0; i < n; i++) {
        filenames[i] = malloc(256 * sizeof(char));
        snprintf(filenames[i], 256, "%s_%d.dat", FILE_PATH_BASE, i);
    }
}

void fill_rand_buffer(char *buf, size_t size) {
    unsigned int seed = time(NULL) ^ (uintptr_t)pthread_self();
    for (size_t i = 0; i < size; ++i) {
        buf[i] = rand_r(&seed) % 256;
    }
}

// 创建文件
void create_test_file(char *filename, size_t file_size) {
    printf("Creating test file: %s\n", filename);
    size_t block_size = _1MB_BYTES * 4;
    ll block_count = file_size / block_size;

    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open for create");
        exit(1);
    }
    char *buf = malloc(block_size);

    for (size_t i = 0; i < block_count; ++i) {
        fill_rand_buffer(buf, block_size);
        if (write(fd, buf, block_size) != block_size) {
            perror("write");
            exit(1);
        }
    }
    close(fd);
    free(buf);
    printf("Test file %s created.\n", filename);
}

void rm_file_if_exists(const char *path) {
    if (access(path, F_OK) == 0) {
        if (remove(path) != 0) {
            perror("remove existing file");
            exit(1);
        }
    }
}

void clear_test_file(char *file_name) {
    printf("Removing test file: %s\n", file_name);
    rm_file_if_exists(file_name);
    printf("Test file removed.\n");
}

void *test_seq_read_job(void *arg) {
    struct test_info *info = (struct test_info *)arg;
    int fd = info->fd;
    size_t io_size = info->io_size;
    size_t file_size = info->file_size;
    void *buf = info->buf;
    size_t block_count = file_size / io_size;
    int iter_count = info->iter_count;

    size_t total_bytes = 0;

    for (ll i = 0; i < iter_count; i++) {
        // printf("Reading iteration %d...\n", i);
        for (int j = 0; j < block_count; j++) {
            size_t bytes_read = read(fd, buf, io_size);
            if (bytes_read < 0) {
                perror("read");
                exit(1);
            }
            total_bytes += bytes_read;
        }
        lseek(fd, 0, SEEK_SET);
    }
    info->total_bytes = total_bytes;
    return NULL;
}

void *test_seq_write_job(void *arg) {
    struct test_info *info = (struct test_info *)arg;
    int fd = info->fd;
    size_t io_size = info->io_size;
    size_t file_size = info->file_size;
    void *buf = info->buf;
    size_t block_count = file_size / io_size;
    int iter_count = info->iter_count;

    size_t total_bytes = 0;

    for (ll i = 0; i < iter_count; i++) {
        for (int j = 0; j < block_count; j++) {
            size_t bytes_written = write(fd, buf, io_size);
            if (bytes_written < 0) {
                perror("write");
                exit(1);
            }
            total_bytes += bytes_written;
        }
        lseek(fd, 0, SEEK_SET);
    }
    info->total_bytes = total_bytes;
    return NULL;
}

void *test_rand_read_job(void *arg) {
    struct test_info *info = (struct test_info *)arg;
    int fd = info->fd;
    size_t io_size = info->io_size;
    size_t file_size = info->file_size;
    void *buf = info->buf;
    size_t block_count = file_size / io_size;
    int iter_count = info->iter_count;

    size_t total_bytes = 0;
    unsigned int seed = time(NULL) + pthread_self();
    for (ll i = 0; i < iter_count; i++) {
        // Randomly seek to a block
        for (int j = 0; j < block_count; j++) {
            off_t offset = (rand_r(&seed) % block_count) * io_size;
            lseek(fd, offset, SEEK_SET);
            size_t bytes_read = read(fd, buf, io_size);
            if (bytes_read < 0) {
                perror("read");
                exit(1);
            }
            total_bytes += bytes_read;
        }
    }
    info->total_bytes = total_bytes;
    return NULL;
}

void *test_rand_write_job(void *arg) {
    struct test_info *info = (struct test_info *)arg;
    int fd = info->fd;
    size_t io_size = info->io_size;
    size_t file_size = info->file_size;
    void *buf = info->buf;
    size_t block_count = file_size / io_size;
    int iter_count = info->iter_count;

    size_t total_bytes = 0;
    unsigned int seed = time(NULL) + pthread_self();
    for (ll i = 0; i < iter_count; i++) {
        for (int j = 0; j < block_count; j++) {
            off_t offset = (rand_r(&seed) % block_count) * io_size;
            lseek(fd, offset, SEEK_SET);
            size_t bytes_written = write(fd, buf, io_size);
            if (bytes_written < 0) {
                perror("write");
                exit(1);
            }
            total_bytes += bytes_written;
        }
    }
    info->total_bytes = total_bytes;
    return NULL;
}

// job_n个线程同时对n个文件进行测试
double mul_thread_test(int job_n, size_t io_size, size_t file_size,
                       int iter_count, enum test_type type) {
    void *(*test_job)(void *) = NULL;
    char *type_str;
    int open_flags;
    switch (type) {
        case SEQ_READ:
            test_job = test_seq_read_job;
            type_str = "Sequential Read";
            open_flags = O_RDONLY;
            break;
        case SEQ_WRITE:
            test_job = test_seq_write_job;
            type_str = "Sequential Write";
            open_flags = O_WRONLY | O_CREAT;
            break;
        case RAND_READ:
            test_job = test_rand_read_job;
            type_str = "Random Read";
            open_flags = O_RDONLY;
            break;
        case RAND_WRITE:
            test_job = test_rand_write_job;
            type_str = "Random Write";
            open_flags = O_WRONLY | O_CREAT;
            break;
    }

    struct test_info test_infos[job_n];
    for (int i = 0; i < job_n; i++) {
        test_infos[i].file_name = filenames[i];
        test_infos[i].fd = open(filenames[i], open_flags);
        if (test_infos[i].fd < 0) {
            perror("open for read");
            exit(1);
        }
        posix_memalign((void **)&test_infos[i].buf, io_size, io_size);
        test_infos[i].file_size = file_size;
        test_infos[i].io_size = io_size;
        test_infos[i].total_bytes = 0;
        test_infos[i].iter_count = iter_count;
    }
    pthread_t threads[job_n];

    struct timespec start, end;
    size_t total_bytes = 0;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (int i = 0; i < job_n; i++) {
        pthread_create(&threads[i], NULL, test_job, &test_infos[i]);
    }
    for (int i = 0; i < job_n; i++) {
        pthread_join(threads[i], NULL);
        total_bytes += test_infos[i].total_bytes;
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    double duration = calculate_time_diff_ns(&start, &end);
    double throughput = (total_bytes / (1024.0 * 1024.0)) /
                        (duration / (double)NANOS_PER_SECOND);

    for (int i = 0; i < job_n; i++) {
        close(test_infos[i].fd);
        free(test_infos[i].buf);
    }
    printf("IO Size %zu , %d jobs, test type: %s, throughput: %.2f MB/s\n",
           io_size, job_n, type_str, throughput);
    return throughput;
}

int main() {
    /* ------设置参数-----*/
    // 最大线程数
    int jobs_max = 64;
    // iter_count是每个线程对整个文件读写次数，因为多线程情况会导致总的读写数量过多，所以在测试时动态调整，不作为固定值
    int iter_count = 5;
    // 测试的iosizse
    size_t iosize[] = {_1KB_BYTES, _1KB_BYTES * 2, _1KB_BYTES * 4,
                       _1KB_BYTES * 8, _1MB_BYTES * 2};
    
    log_fp = fopen(log_filename, "w");
    fprintf(log_fp,
            "file_size, "
            "io_size,thread_count,test_type,run_time(s),throughput_MB_per_s\n");
    /*-------------*/
    printf("==============================================================\n");
    printf("Starting performance tests on files with max %d threads...\n",
           jobs_max);
    printf("IO sizes to test: ");
    for (int i = 0; i < sizeof(iosize) / sizeof(iosize[0]); i++) {
        printf("%zu ", iosize[i]);
    }
    printf("\n");

    printf(
        "==============================================================\n\n");
    init_filenames(jobs_max);


    for (int i = 0; i < jobs_max; i++) {
        create_test_file(filenames[i], FILE_SIZE);
    }
    printf("=======Start test=======\n");

    for (int _jobn = 1; _jobn <= jobs_max; _jobn *= 2) {
        printf("\n====== Testing with %d threads ======\n", _jobn);
        for (int i = 0; i < sizeof(iosize) / sizeof(iosize[0]); i++) {
            size_t io_size = iosize[i];
            printf("\n--- Testing with IO size: %zu bytes, %d threads ---\n",
                   io_size, _jobn);
            mul_thread_test(_jobn, io_size, FILE_SIZE, iter_count, SEQ_READ);
            mul_thread_test(_jobn, io_size, FILE_SIZE, iter_count, SEQ_WRITE);
            mul_thread_test(_jobn, io_size, FILE_SIZE, iter_count, RAND_READ);
            mul_thread_test(_jobn, io_size, FILE_SIZE, iter_count, RAND_WRITE);
        }
        // iter_count--;
        if (iter_count < 1)
            iter_count = 1;
    }
    printf("=======Test finished=======\n");

    for (int i = 0; i < jobs_max; i++)
        clear_test_file(filenames[i]);
    return 0;
}