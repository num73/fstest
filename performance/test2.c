/*
    基础性能测试，包括顺序读、顺序写、随机读、随机写等场景
    测试函数使用posix的read，write

    文件大小固定，不同的io_size对性能的影响测试。
*/

#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef long long ll;
#define _1KB_BYTES (1024)
#define _1MB_BYTES (1024 * 1024)

#define FILE_PATH "/mnt/nufs/fs_testfile.dat"
#define FILE_SIZE (1024 * 1024 * 1024L)  // 1GB

// #define BLOCK_COUNT (FILE_SIZE / BLOCK_SIZE)

#define N (10)

#define NANOS_PER_SECOND (1000000000L)

// 计算是时间差，单位ns
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

void create_test_file() {
    printf("Creating test file: %s\n", FILE_PATH);
    size_t block_size = _1KB_BYTES * 4;
    ll block_count = FILE_SIZE / block_size;

    int fd = open(FILE_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0644);
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

// test seq read
double test_seq_read(size_t io_size) {
    ll block_count = FILE_SIZE / io_size;
    int fd = open(FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open for read");
        exit(1);
    }
    char *buf = malloc(io_size);
    struct timespec start, end;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        // printf("Reading iteration %d...\n", i);
        for (int j = 0; j < block_count; j++) {
            if (read(fd, buf, io_size) != io_size) {
                perror("read");
                exit(1);
            }
        }
        lseek(fd, 0, SEEK_SET);
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);
    double duration = calculate_time_diff_ns(&start, &end);
    double throughput = (N * FILE_SIZE / (1024.0 * 1024.0)) /
                        (duration / (double)NANOS_PER_SECOND);
    printf("IO size %zu Sequential read throughput: %.2f MB/s\n", io_size,
           throughput);

    close(fd);
    free(buf);
    return throughput;
}

double test_seq_write(size_t io_size) {
    ll block_count = FILE_SIZE / io_size;
    int fd = open(FILE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("open for write");
        exit(1);
    }
    char *buf = malloc(io_size);
    fill_rand_buffer(buf, io_size);
    struct timespec start, end;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        for (int j = 0; j < block_count; j++) {
            if (write(fd, buf, io_size) != io_size) {
                perror("write");
                exit(1);
            }
        }
        lseek(fd, 0, SEEK_SET);
    }
    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    double duration = calculate_time_diff_ns(&start, &end);
    double throughput = (N * FILE_SIZE / (1024.0 * 1024.0)) /
                        (duration / (double)NANOS_PER_SECOND);
    printf("IO size %zu Sequential write throughput: %.2f MB/s\n", io_size,
           throughput);

    close(fd);
    free(buf);
    return throughput;
}

double test_random_read(size_t io_size) {
    ll block_count = FILE_SIZE / io_size;
    int fd = open(FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open for read");
        exit(1);
    }
    char *buf = malloc(io_size);
    struct timespec start, end;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        // Randomly seek to a block
        for (int j = 0; j < block_count; j++) {
            off_t offset = (rand() % block_count) * io_size;
            lseek(fd, offset, SEEK_SET);
            if (read(fd, buf, io_size) != io_size) {
                perror("read");
                exit(1);
            }
        }
    }
    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    double duration = calculate_time_diff_ns(&start, &end);
    double throughput = (N * FILE_SIZE / (1024.0 * 1024.0)) /
                        (duration / (double)NANOS_PER_SECOND);
    printf("IO size %zu Random read throughput: %.2f MB/s\n", io_size,
           throughput);
    close(fd);
    free(buf);
    return throughput;
}

double test_random_write(size_t io_size) {
    ll block_count = FILE_SIZE / io_size;
    int fd = open(FILE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("open for write");
        exit(1);
    }
    char *buf = malloc(io_size);
    fill_rand_buffer(buf, io_size);
    struct timespec start, end;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        for (int j = 0; j < block_count; j++) {
            off_t offset = (rand() % block_count) * io_size;
            lseek(fd, offset, SEEK_SET);
            if (write(fd, buf, io_size) != io_size) {
                perror("write");
                exit(1);
            }
        }
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    double duration = calculate_time_diff_ns(&start, &end);
    double throughput = (N * FILE_SIZE / (1024.0 * 1024.0)) /
                        (duration / (double)NANOS_PER_SECOND);
    printf("IO Size %zu Random write throughput: %.2f MB/s\n", io_size,
           throughput);
    close(fd);
    free(buf);
    return throughput;
}

void rm_file_if_exists(const char *path) {
    if (access(path, F_OK) == 0) {
        if (remove(path) != 0) {
            perror("remove existing file");
            exit(1);
        }
    }
}

void clear_test_file() {
    rm_file_if_exists(FILE_PATH);
}

int main() {
    printf("Starting performance tests on file: %s\n", FILE_PATH);
    size_t iosize[] = {_1KB_BYTES, _1KB_BYTES * 2, _1KB_BYTES * 4,
                       _1KB_BYTES * 8};

    create_test_file();

    for (int i = 0; i < sizeof(iosize) / sizeof(iosize[0]); i++) {
        size_t io_size = iosize[i];
        printf("\n--- Testing with IO size: %zu bytes ---\n", io_size);
        test_seq_read(io_size);
        test_seq_write(io_size);
        test_random_read(io_size);
        test_random_write(io_size);
    }

    clear_test_file();
    printf("Performance tests completed.\n");

    return 0;
}