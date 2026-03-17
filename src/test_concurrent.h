/*
    并发测试模块
    验证多个进程或线程同时操作时是否可靠
*/

#ifndef FSTEST_TEST_CONCURRENT_H
#define FSTEST_TEST_CONCURRENT_H

#include "common.h"

void run_concurrent_tests(const struct fstest_config *cfg);

#endif /* FSTEST_TEST_CONCURRENT_H */
