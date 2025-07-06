#ifndef __TOOLS_H
#define __TOOLS_H

#include <stdio.h>

#define _1G_BYTES (1073741824L)  // 1024 * 1024 * 1024
#define _1M_BYTES (1048576L)     // 1024 * 1024
#define _1K_BYTES (1024)
#define _1S_NS (1000000000L)

double get_time_ns();

void random_char_fill(char *buf, size_t size);

// 交换两个元素
void swap(size_t *a, size_t *b);

// Fisher-Yates洗牌算法
void shuffle_array(size_t arr[], size_t n);

#endif  // __TOOLS_H