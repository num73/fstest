/*
    压力和稳定性测试模块
    测试系统在高负载下的表现
*/

#ifndef FSTEST_TEST_STRESS_H
#define FSTEST_TEST_STRESS_H

#include "common.h"

void run_stress_tests(const struct fstest_config *cfg);

#endif /* FSTEST_TEST_STRESS_H */
