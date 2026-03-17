/*
    数据一致性测试模块
    确保写入的数据读回后是正确的
*/

#ifndef FSTEST_TEST_CONSISTENCY_H
#define FSTEST_TEST_CONSISTENCY_H

#include "common.h"

void run_consistency_tests(const struct fstest_config *cfg);

#endif /* FSTEST_TEST_CONSISTENCY_H */
