
#include "tools.h"

#include <stdlib.h>
#include <time.h>

double get_time_ns() {
    struct timespec ts;
    __sync_synchronize();
    clock_gettime(CLOCK_MONOTONIC, &ts);
    __sync_synchronize();
    return (double)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

void random_char_fill(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = 'a' + (rand() % 26);
    }
}

// 交换两个元素
void swap(size_t *a, size_t *b) {
    size_t temp = *a;
    *a = *b;
    *b = temp;
}

// Fisher-Yates洗牌算法
void shuffle_array(size_t arr[], size_t n) {
    for (size_t i = n - 1; i > 0; i--) {
        // 生成0到i之间的随机数
        size_t j = rand() % (i + 1);
        swap(&arr[i], &arr[j]);
    }
}
