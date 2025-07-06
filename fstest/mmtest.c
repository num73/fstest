#include "mmtest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools.h"

void mm_seq_read_test(size_t length, size_t iosize, int per_hot) {
    char *va = malloc(length);
    char *buf = malloc(iosize);

    random_char_fill(buf, iosize);
    if (per_hot) {
        memset(va, 0, length);  // 确保每个页面都被访问过
    }

    long start = get_time_ns();
    for (size_t i = 0; i < length; i += iosize) {
        memcpy(buf, va + i, iosize);
    }
    long end = get_time_ns();
    double time_spent_s = (double)(end - start) / _1S_NS;
    if (!per_hot) {
        printf("mm_seq_read_test: Each page accessed once, ");
    } else {
        printf("mm_seq_read_test: Each page accessed multiple times, ");
    }
    printf("mm_seq_read_test: Throughput: %.2f MB/s\n",
           (double)length / _1M_BYTES / time_spent_s);

    free(buf);
    free(va);
}

void mm_seq_write_test(size_t length, size_t iosize, int per_hot) {
    char *va = malloc(length);
    char *buf = malloc(iosize);

    random_char_fill(buf, iosize);
    if (per_hot) {
        memset(va, 0, length);  // 确保每个页面都被访问过
    }

    long start = get_time_ns();
    for (size_t i = 0; i < length; i += iosize) {
        memcpy(va + i, buf, iosize);
    }
    long end = get_time_ns();
    double time_spent_s = (double)(end - start) / _1S_NS;
    if (!per_hot) {
        printf("mm_seq_read_test: Each page accessed once, ");
    } else {
        printf("mm_seq_read_test: Each page accessed multiple times, ");
    }
    printf("mm_seq_write_test: Throughput: %.2f MB/s\n",
           (double)length / _1M_BYTES / time_spent_s);
    free(buf);
    free(va);
}

void mm_rand_read_test(size_t length, size_t iosize, int per_hot) {
    char *va = malloc(length);
    char *buf = malloc(iosize);

    size_t *ind = (size_t *)malloc(length / iosize * sizeof(size_t));
    for (size_t i = 0; i < length / iosize; i++) {
        ind[i] = i * iosize;
    }
    shuffle_array(ind, length / iosize);

    random_char_fill(buf, iosize);
    if (per_hot) {
        memset(va, 0, length);  // 确保每个页面都被访问过
    }
    long start = get_time_ns();
    for (size_t i = 0; i < length / iosize; i++) {
        memcpy(buf, va + ind[i], iosize);
    }
    long end = get_time_ns();
    double time_spent_s = (double)(end - start) / _1S_NS;
    if (!per_hot) {
        printf("mm_seq_read_test: Each page accessed once, ");
    } else {
        printf("mm_seq_read_test: Each page accessed multiple times, ");
    }
    printf("mm_rand_read_test: Throughput: %.2f MB/s\n",
           (double)length / _1M_BYTES / time_spent_s);

    free(ind);
    free(buf);
    free(va);
}

void mm_rand_write_test(size_t length, size_t iosize, int per_hot) {
    char *va = malloc(length);
    char *buf = malloc(iosize);

    size_t *ind = (size_t *)malloc(length / iosize * sizeof(size_t));
    for (size_t i = 0; i < length / iosize; i++) {
        ind[i] = i * iosize;
    }
    shuffle_array(ind, length / iosize);

    random_char_fill(buf, iosize);

    if (per_hot) {
        memset(va, 0, length);  // 确保每个页面都被访问过
    }

    long start = get_time_ns();
    for (size_t i = 0; i < length / iosize; i++) {
        memcpy(va + ind[i], buf, iosize);
    }
    long end = get_time_ns();
    double time_spent_s = (double)(end - start) / _1S_NS;
    if (!per_hot) {
        printf("mm_seq_read_test: Each page accessed once, ");
    } else {
        printf("mm_seq_read_test: Each page accessed multiple times, ");
    }
    printf("mm_rand_write_test: Throughput: %.2f MB/s\n",
           (double)length / _1M_BYTES / time_spent_s);

    free(ind);
    free(buf);
    free(va);
}