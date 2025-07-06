#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "mmtest.h"
#include "tools.h"

#define DEV_PATH "/dev/dax0.0"
#define IO_SIZE_BYTES 4096   // 4KB
#define LENGTH (1073741824)  // 1024 * 1024 * 1024

#define INPLACE 1

void create_file(const char *file_path, size_t length) {
    int fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        return;
    }
    if (ftruncate(fd, length) < 0) {
        perror("ftruncate");
        close(fd);
        return;
    }
    close(fd);
}

void fill_file_with_random_data(const char *file_path, size_t length) {
    int fd = open(file_path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return;
    }

    char *buf = (char *)malloc(IO_SIZE_BYTES);
    if (!buf) {
        perror("malloc");
        close(fd);
        return;
    }
    random_char_fill(buf, IO_SIZE_BYTES);

    for (size_t i = 0; i < length; i += IO_SIZE_BYTES) {
        if (write(fd, buf, IO_SIZE_BYTES) < 0) {
            perror("write");
            free(buf);
            close(fd);
            return;
        }
    }

    free(buf);
    close(fd);
}

void test_seq_inplace_mmap_write(char *file_path, size_t length,
                                 size_t iosize) {
    int fd = open(file_path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return;
    }

    char *mmap_addr =
        mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_addr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }
    char *buf = (char *)malloc(IO_SIZE_BYTES);
    random_char_fill(buf, IO_SIZE_BYTES);
    long start = get_time_ns();
    for (size_t i = 0; i < length; i += iosize) {
        memcpy(mmap_addr + i, buf, iosize);
    }
    long end = get_time_ns();
    double time_spent_s = (double)(end - start) / _1S_NS;

    printf("Seq Mmap Write Throughput: %.2f MB/s\n",
           (double)length / _1M_BYTES / time_spent_s);

    munmap(mmap_addr, length);
    free(buf);
    close(fd);
}

void test_seq_inplace_mmap_read(char *file_path, size_t length, size_t iosize) {
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    char *mmap_addr = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
    if (mmap_addr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }
    char *buf = (char *)malloc(iosize);
    long start = get_time_ns();
    for (size_t i = 0; i < length; i += iosize) {
        memcpy(buf, mmap_addr + i, iosize);
    }
    long end = get_time_ns();
    double time_spent_s = (double)(end - start) / _1S_NS;

    printf("Seq Mmap Read Throughput: %.2f MB/s\n",
           (double)length / _1M_BYTES / time_spent_s);

    munmap(mmap_addr, length);
    free(buf);
    close(fd);
}

void test_rand_inplace_mmap_read(char *file_path, size_t length,
                                 size_t iosize) {
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    char *mmap_addr = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
    if (mmap_addr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }
    size_t *ind = (size_t *)malloc(length / iosize * sizeof(size_t));

    for (size_t i = 0; i < length / iosize; i++) {
        ind[i] = i * iosize;
    }

    shuffle_array(ind, length / iosize);

    char *buf = (char *)malloc(iosize);
    long start = get_time_ns();
    for (size_t i = 0; i < length / iosize; i++) {
        memcpy(buf, mmap_addr + ind[i], iosize);
    }
    long end = get_time_ns();
    double time_spent_s = (double)(end - start) / _1S_NS;

    printf("Rand Mmap Read Throughput: %.2f MB/s\n",
           (double)length / _1M_BYTES / time_spent_s);

    munmap(mmap_addr, length);
    free(buf);
    free(ind);
    close(fd);
}

void test_rand_inplace_mmap_write(char *file_path, size_t length,
                                  size_t iosize) {
    int fd = open(file_path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return;
    }

    char *mmap_addr = mmap(NULL, length, PROT_WRITE, MAP_SHARED, fd, 0);

    size_t *ind = (size_t *)malloc(length / iosize * sizeof(size_t));

    for (size_t i = 0; i < length / iosize; i++) {
        ind[i] = i * iosize;
    }

    shuffle_array(ind, length / iosize);

    char *buf = (char *)malloc(iosize);
    random_char_fill(buf, iosize);
    long start = get_time_ns();

    for (size_t i = 0; i < length / iosize; i++) {
        memcpy(mmap_addr + ind[i], buf, iosize);
    }
    long end = get_time_ns();
    double time_spent_s = (double)(end - start) / _1S_NS;

    printf("Rand Mmap Write Throughput: %.2f MB/s\n",
           (double)length / _1M_BYTES / time_spent_s);

    munmap(mmap_addr, length);
    free(buf);
    free(ind);
    close(fd);
}

int main() {
    srand(time(NULL));
    test_seq_inplace_mmap_read(DEV_PATH, 3 * _1G_BYTES, IO_SIZE_BYTES);
    test_seq_inplace_mmap_write(DEV_PATH, 3 * _1G_BYTES, IO_SIZE_BYTES);

    test_rand_inplace_mmap_read(DEV_PATH, 3 * _1G_BYTES, IO_SIZE_BYTES);
    test_rand_inplace_mmap_write(DEV_PATH, 3 * _1G_BYTES, IO_SIZE_BYTES);

    mm_seq_read_test(3 * _1G_BYTES, IO_SIZE_BYTES, 0);
    mm_seq_write_test(3 * _1G_BYTES, IO_SIZE_BYTES, 0);
    mm_rand_read_test(3 * _1G_BYTES, IO_SIZE_BYTES, 0);
    mm_rand_write_test(3 * _1G_BYTES, IO_SIZE_BYTES, 0);

    mm_seq_read_test(3 * _1G_BYTES, IO_SIZE_BYTES, 1);
    mm_seq_write_test(3 * _1G_BYTES, IO_SIZE_BYTES, 1);
    mm_rand_read_test(3 * _1G_BYTES, IO_SIZE_BYTES, 1);
    mm_rand_write_test(3 * _1G_BYTES, IO_SIZE_BYTES, 1);
    return 0;
}