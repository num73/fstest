#ifndef MMTEST_H
#define MMTEST_H

#include <stdio.h>

void mm_seq_read_test(size_t length, size_t iosize, int per_hot);
void mm_seq_write_test(size_t length, size_t iosize, int per_hot);

void mm_rand_read_test(size_t length, size_t iosize, int per_hot);
void mm_rand_write_test(size_t length, size_t iosize, int per_hot);

#endif  // MMTEST_H