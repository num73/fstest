/*
    基础性能测试，包括顺序读、顺序写、随机读、随机写等场景
    测试函数使用posix的read，write
    读写混合场景测试

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

#define _1KB_BYTES (1024L)
#define _1MB_BYTES (1024 * 1024L)

#define _1GB_BYTES (1024 * 1024 * 1024L)

#define FILE_PATH "/mnt/nufs/fs_testfile.dat"
#define FILE_SIZE (1 * _1GB_BYTES)   // 1GB
#define BLOCK_SIZE (4 * _1KB_BYTES)  // 4KB
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

    for (size_t i = 0; i < BLOCK_COUNT; ++i) {
        fill_rand_buffer(buf, BLOCK_SIZE);
        if (write(fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("write");
            exit(1);
        }
    }
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

void mix_read_write(double write_ratio) {
    srand(time(NULL));
    // 读写混合测试

    int fd = open(FILE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open for read/write");
        exit(1);
    }

    char *buf = malloc(BLOCK_SIZE);
    struct timespec start, end;
    size_t total_bytes = 0;
    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &start);
    atomic_thread_fence(memory_order_seq_cst);

    for (ll i = 0; i < N; i++) {
        // printf("Reading iteration %d...\n", i);
        for (int j = 0; j < BLOCK_COUNT; j++) {
            int rand_n = rand();
            off_t offset = (rand_n % BLOCK_COUNT) * BLOCK_SIZE;
            lseek(fd, offset, SEEK_SET);
            double r = (double)rand_n / RAND_MAX;
            size_t io_cnt = 0;
            if (r < write_ratio) {
                io_cnt += write(fd, buf, BLOCK_SIZE);
                // Write
                if (io_cnt < 0) {
                    perror("write");
                    exit(1);
                }
            } else {
                io_cnt += read(fd, buf, BLOCK_SIZE);
                // Read
                if (io_cnt != BLOCK_SIZE) {
                    perror("read");
                    exit(1);
                }
            }
            total_bytes += io_cnt;
        }
    }

    atomic_thread_fence(memory_order_seq_cst);
    clock_gettime(CLOCK_MONOTONIC, &end);
    atomic_thread_fence(memory_order_seq_cst);

    double duration = calculate_time_diff_ns(&start, &end);
    printf("Read/Write mix ratio %.2f/%.2f throughput: %.2f MB/s\n",
           1.0 - write_ratio, write_ratio,
           (total_bytes / (1024.0 * 1024.0)) /
               (duration / (double)NANOS_PER_SECOND));

    close(fd);
    free(buf);
}

void clear_test_file() {
    rm_file_if_exists(FILE_PATH);
}

int main() {
    printf("Creating test file of size %d bytes...\n", FILE_SIZE);
    create_test_file();
    mix_read_write(0.0);
    mix_read_write(0.25);
    mix_read_write(0.5);
    mix_read_write(0.75);
    mix_read_write(1.0);
    printf("Test finished. Cleaning up...\n");
    clear_test_file();

    return 0;
}