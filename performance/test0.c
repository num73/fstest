/*
    基础性能测试，包括顺序读、顺序写、随机读、随机写等场景
    测试函数使用posix的read，write


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

#define FILE_PATH "fs_testfile.dat"
#define FILE_SIZE (1024 * 1024 * 1024L)  // 1GB
#define BLOCK_SIZE (4096)                // 4KB
#define BLOCK_COUNT (FILE_SIZE / BLOCK_SIZE)

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
    int fd = open(FILE_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open for create");
        exit(1);
    }
    char *buf = malloc(BLOCK_SIZE);
    fill_rand_buffer(buf, BLOCK_SIZE);
    for (size_t i = 0; i < BLOCK_COUNT; ++i) {
        if (write(fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("write");
            exit(1);
        }
    }
    close(fd);
    free(buf);
}

// test seq read
void test_seq_read() {
    int fd = open(FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open for read");
        exit(1);
    }
    char *buf = malloc(BLOCK_SIZE);
    struct timespec start, end;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        // printf("Reading iteration %d...\n", i);
        for (int j = 0; j < BLOCK_COUNT; j++) {
            if (read(fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
                perror("read");
                exit(1);
            }
        }
        lseek(fd, 0, SEEK_SET);
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    printf(
        "Sequential read throughput: %.2f MB/s\n",
        (N * FILE_SIZE / (1024.0 * 1024.0)) /
            (calculate_time_diff_ns(&start, &end) / (double)NANOS_PER_SECOND));

    close(fd);
    free(buf);
}

void test_seq_write() {
    int fd = open(FILE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("open for write");
        exit(1);
    }
    char *buf = malloc(BLOCK_SIZE);
    fill_rand_buffer(buf, BLOCK_SIZE);
    struct timespec start, end;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        for (int j = 0; j < BLOCK_COUNT; j++) {
            if (write(fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
                perror("write");
                exit(1);
            }
        }
        lseek(fd, 0, SEEK_SET);
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    printf(
        "Sequential write throughput: %.2f MB/s\n",
        (N * FILE_SIZE / (1024.0 * 1024.0)) /
            (calculate_time_diff_ns(&start, &end) / (double)NANOS_PER_SECOND));

    close(fd);
    free(buf);
}

void test_random_read() {
    int fd = open(FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open for read");
        exit(1);
    }
    char *buf = malloc(BLOCK_SIZE);
    struct timespec start, end;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        // Randomly seek to a block
        for (int j = 0; j < BLOCK_COUNT; j++) {
            off_t offset = (rand() % BLOCK_COUNT) * BLOCK_SIZE;
            lseek(fd, offset, SEEK_SET);
            if (read(fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
                perror("read");
                exit(1);
            }
        }
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);
    printf(
        "Random read throughput: %.2f MB/s\n",
        (N * FILE_SIZE / (1024.0 * 1024.0)) /
            (calculate_time_diff_ns(&start, &end) / (double)NANOS_PER_SECOND));
    close(fd);
    free(buf);
}

void test_random_write() {
    int fd = open(FILE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("open for write");
        exit(1);
    }
    char *buf = malloc(BLOCK_SIZE);
    fill_rand_buffer(buf, BLOCK_SIZE);
    struct timespec start, end;

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        for (int j = 0; j < BLOCK_COUNT; j++) {
            off_t offset = (rand() % BLOCK_COUNT) * BLOCK_SIZE;
            lseek(fd, offset, SEEK_SET);
            if (write(fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
                perror("write");
                exit(1);
            }
        }
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);
    printf(
        "Random write throughput: %.2f MB/s\n",
        (N * FILE_SIZE / (1024.0 * 1024.0)) /
            (calculate_time_diff_ns(&start, &end) / (double)NANOS_PER_SECOND));
    close(fd);
    free(buf);
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
    printf("Creating test file of size %d bytes...\n", FILE_SIZE);
    create_test_file();
    test_seq_read();
    test_seq_write();
    test_random_read();
    test_random_write();
    clear_test_file();

    return 0;
}