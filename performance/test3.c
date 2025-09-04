/*
    基础性能测试，包括顺序读、顺序写、随机读、随机写等场景
    测试函数使用posix的read，write

    IO SIZE固定，不同的线程数对性能的影响测试。
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

#define FILE_PATH_BASE "/mnt/nufs/fs_testfile"
#define FILE_SIZE (256 * _1MB_BYTES)

// #define BLOCK_COUNT (FILE_SIZE / BLOCK_SIZE)

#define N (10)

#define NANOS_PER_SECOND (1000000000L)

char **filenames;
enum test_type { SEQ_READ, SEQ_WRITE, RAND_READ, RAND_WRITE };

struct test_info {
    char *file_name;
    int fd;
    void *buf;
    size_t file_size;
    size_t io_size;
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

    printf("test files: ");
}

void fill_rand_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        buf[i] = rand() % 256;
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
    printf("Test file created.\n");
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

    size_t total_bytes = 0;

    for (ll i = 0; i < N; i++) {
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

    size_t total_bytes = 0;

    for (ll i = 0; i < N; i++) {
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

    size_t total_bytes = 0;

    for (ll i = 0; i < N; i++) {
        // Randomly seek to a block
        for (int j = 0; j < block_count; j++) {
            off_t offset = (rand() % block_count) * io_size;
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

    size_t total_bytes = 0;

    for (ll i = 0; i < N; i++) {
        for (int j = 0; j < block_count; j++) {
            off_t offset = (rand() % block_count) * io_size;
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

// n个线程同时对n个文件进行测试
double mul_thread_test(int job_n, size_t io_size, size_t file_size,
                       enum test_type type) {
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
            open_flags = O_WRONLY | O_CREAT | O_TRUNC;
            break;
        case RAND_READ:
            test_job = test_rand_read_job;
            type_str = "Random Read";
            open_flags = O_RDONLY;
            break;
        case RAND_WRITE:
            test_job = test_rand_write_job;
            type_str = "Random Write";
            open_flags = O_WRONLY | O_CREAT | O_TRUNC;
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

    printf("IO Size %zu , %d jobs, test type: %s, throughput: %.2f MB/s\n",
           io_size, job_n, type_str, throughput);
    return throughput;
}

int main() {
    /* ------设置参数-----*/
    int jobs_n = 4;
    /*-------------*/

    init_filenames(jobs_n);
    size_t iosize[] = {_1KB_BYTES, _1KB_BYTES * 2, _1KB_BYTES * 4,
                       _1KB_BYTES * 8};

    for (int i = 0; i < jobs_n; i++) {
        create_test_file(filenames[i], FILE_SIZE);
    }
    printf("=======Start test=======\n");
    mul_thread_test(jobs_n, _1KB_BYTES * 4, FILE_SIZE, SEQ_READ);
    mul_thread_test(jobs_n, _1KB_BYTES * 4, FILE_SIZE, SEQ_WRITE);
    mul_thread_test(jobs_n, _1KB_BYTES * 4, FILE_SIZE, RAND_READ);
    mul_thread_test(jobs_n, _1KB_BYTES * 4, FILE_SIZE, RAND_WRITE);
    printf("=======Test finished=======\n");

    for (int i = 0; i < jobs_n; i++)
        clear_test_file(filenames[i]);
    return 0;
}