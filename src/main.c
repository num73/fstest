/*
    fstest - 文件系统综合测试工具
    单个可执行程序，通过选项来选择功能

    使用方法：
    ./fstest -d <测试目录> [-m <模式>] [-j <线程数>] [-s <IO大小>]
             [-f <文件大小MB>] [-i <迭代次数>] [-v]

    测试模式 (-m):
            all         : 运行所有测试 (默认)
            functional  : 功能正确性测试
            consistency : 数据一致性测试
            exception   : 异常场景测试
            concurrent  : 并发测试
            stress      : 压力和稳定性测试
            performance : 性能测试

    示例：
            ./fstest -d /tmp/fstest_data -m all                 # 运行所有测试
            ./fstest -d /mnt/nufs -m performance -j 4 -s 4096  # 性能测试，4线程
            ./fstest -d /tmp/fstest_data -m functional         # 仅功能正确性测试
*/

#include "common.h"
#include "test_concurrent.h"
#include "test_consistency.h"
#include "test_exception.h"
#include "test_functional.h"
#include "test_performance.h"
#include "test_stress.h"

static void print_usage(const char *prog) {
    printf("fstest - 文件系统综合测试工具\n\n");
    printf("Usage: %s -d <dir> [options]\n\n", prog);
    printf("Required:\n");
    printf("  -d <dir>     测试文件存放目录\n\n");
    printf("Options:\n");
    printf("  -m <mode>    测试模式 (默认: all)\n");
    printf("                 all = 运行所有测试\n");
    printf("                 functional = 功能正确性测试\n");
    printf("                 consistency = 数据一致性测试\n");
    printf("                 exception = 异常场景测试\n");
    printf("                 concurrent = 并发测试\n");
    printf("                 stress = 压力和稳定性测试\n");
    printf("                 performance = 性能测试\n");
    printf("  -j <n>       并发线程数 (默认: %d, 最大: %d)\n",
           DEFAULT_JOBS, MAX_JOBS);
    printf("  -s <bytes>   IO 大小 (默认: %ld)\n",
           (long)DEFAULT_IO_SIZE);
    printf("  -f <MB>      测试文件大小，单位MB (默认: %ld)\n",
           (long)(DEFAULT_FILE_SIZE / _1MB_BYTES));
    printf("  -i <n>       迭代次数 (默认: %d)\n", DEFAULT_ITER);
    printf("  -v           详细输出\n");
    printf("  -h           显示帮助信息\n");
    printf("\nExamples:\n");
    printf("  %s -d /tmp/fstest_data\n", prog);
    printf("  %s -d /mnt/nufs -m performance -j 4 -s 4096\n", prog);
    printf("  %s -d /tmp/fstest_data -m functional\n", prog);
}

static const char *mode_key(enum fstest_mode mode) {
    switch (mode) {
        case TEST_MODE_ALL: return "all";
        case TEST_MODE_FUNCTIONAL: return "functional";
        case TEST_MODE_CONSISTENCY: return "consistency";
        case TEST_MODE_EXCEPTION: return "exception";
        case TEST_MODE_CONCURRENT: return "concurrent";
        case TEST_MODE_STRESS: return "stress";
        case TEST_MODE_PERFORMANCE: return "performance";
        default: return "unknown";
    }
}

static const char *mode_name(enum fstest_mode mode) {
    switch (mode) {
        case TEST_MODE_ALL: return "所有测试";
        case TEST_MODE_FUNCTIONAL: return "功能正确性测试";
        case TEST_MODE_CONSISTENCY: return "数据一致性测试";
        case TEST_MODE_EXCEPTION: return "异常场景测试";
        case TEST_MODE_CONCURRENT: return "并发测试";
        case TEST_MODE_STRESS: return "压力和稳定性测试";
        case TEST_MODE_PERFORMANCE: return "性能测试";
        default: return "未知";
    }
}

static int parse_test_mode(const char *mode_arg, enum fstest_mode *mode) {
    if (strcasecmp(mode_arg, "all") == 0 || strcmp(mode_arg, "0") == 0) {
        *mode = TEST_MODE_ALL;
        return 0;
    }
    if (strcasecmp(mode_arg, "functional") == 0 || strcmp(mode_arg, "1") == 0) {
        *mode = TEST_MODE_FUNCTIONAL;
        return 0;
    }
    if (strcasecmp(mode_arg, "consistency") == 0 || strcmp(mode_arg, "2") == 0) {
        *mode = TEST_MODE_CONSISTENCY;
        return 0;
    }
    if (strcasecmp(mode_arg, "exception") == 0 || strcmp(mode_arg, "3") == 0) {
        *mode = TEST_MODE_EXCEPTION;
        return 0;
    }
    if (strcasecmp(mode_arg, "concurrent") == 0 || strcmp(mode_arg, "4") == 0) {
        *mode = TEST_MODE_CONCURRENT;
        return 0;
    }
    if (strcasecmp(mode_arg, "stress") == 0 || strcmp(mode_arg, "5") == 0) {
        *mode = TEST_MODE_STRESS;
        return 0;
    }
    if (strcasecmp(mode_arg, "performance") == 0 || strcmp(mode_arg, "6") == 0) {
        *mode = TEST_MODE_PERFORMANCE;
        return 0;
    }

    return -1;
}

int main(int argc, char *argv[]) {
    struct fstest_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.jobs = DEFAULT_JOBS;
    cfg.io_size = DEFAULT_IO_SIZE;
    cfg.file_size = DEFAULT_FILE_SIZE;
    cfg.iter_count = DEFAULT_ITER;
    cfg.test_mode = TEST_MODE_ALL;
    cfg.verbose = 0;

    int opt;
    while ((opt = getopt(argc, argv, "d:m:j:s:f:i:vh")) != -1) {
        switch (opt) {
            case 'd':
                strncpy(cfg.dir, optarg, MAX_PATH_LEN - 1);
                break;
            case 'm':
                if (parse_test_mode(optarg, &cfg.test_mode) != 0) {
                    fprintf(stderr,
                            "Error: 无效的测试模式 '%s'\n"
                            "有效模式: all, functional, consistency, "
                            "exception, concurrent, stress, performance\n",
                            optarg);
                    return 1;
                }
                break;
            case 'j':
                cfg.jobs = atoi(optarg);
                if (cfg.jobs < 1) cfg.jobs = 1;
                if (cfg.jobs > MAX_JOBS) cfg.jobs = MAX_JOBS;
                break;
            case 's':
                cfg.io_size = (size_t)atol(optarg);
                if (cfg.io_size < 512) cfg.io_size = 512;
                break;
            case 'f':
                cfg.file_size = (size_t)atol(optarg) * _1MB_BYTES;
                if (cfg.file_size < _1MB_BYTES)
                    cfg.file_size = _1MB_BYTES;
                break;
            case 'i':
                cfg.iter_count = atoi(optarg);
                if (cfg.iter_count < 1) cfg.iter_count = 1;
                break;
            case 'v':
                cfg.verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (strlen(cfg.dir) == 0) {
        fprintf(stderr, "Error: 必须指定测试目录 (-d)\n\n");
        print_usage(argv[0]);
        return 1;
    }

    /* 确保测试目录存在 */
    if (ensure_dir_exists(cfg.dir) != 0) {
        fprintf(stderr, "Error: 无法创建测试目录 %s: %s\n",
                cfg.dir, strerror(errno));
        return 1;
    }

    /* 打印配置 */
    printf("╔══════════════════════════════════════════╗\n");
    printf("║    fstest - 文件系统综合测试工具         ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("配置:\n");
    printf("  测试目录:   %s\n", cfg.dir);
    printf("  测试模式:   %s (%s)\n", mode_key(cfg.test_mode),
           mode_name(cfg.test_mode));
    printf("  线程数:     %d\n", cfg.jobs);
    printf("  IO 大小:    %zu bytes\n", cfg.io_size);
    printf("  文件大小:   %zu MB\n", cfg.file_size / _1MB_BYTES);
    printf("  迭代次数:   %d\n", cfg.iter_count);

    srand(time(NULL));

    /* 根据模式运行对应测试 */
    struct timespec total_start, total_end;
    clock_gettime(CLOCK_MONOTONIC, &total_start);

    if (cfg.test_mode == TEST_MODE_ALL || cfg.test_mode == TEST_MODE_FUNCTIONAL) {
        run_functional_tests(&cfg);
    }
    if (cfg.test_mode == TEST_MODE_ALL || cfg.test_mode == TEST_MODE_CONSISTENCY) {
        run_consistency_tests(&cfg);
    }
    if (cfg.test_mode == TEST_MODE_ALL || cfg.test_mode == TEST_MODE_EXCEPTION) {
        run_exception_tests(&cfg);
    }
    if (cfg.test_mode == TEST_MODE_ALL || cfg.test_mode == TEST_MODE_CONCURRENT) {
        run_concurrent_tests(&cfg);
    }
    if (cfg.test_mode == TEST_MODE_ALL || cfg.test_mode == TEST_MODE_STRESS) {
        run_stress_tests(&cfg);
    }
    if (cfg.test_mode == TEST_MODE_ALL || cfg.test_mode == TEST_MODE_PERFORMANCE) {
        run_performance_tests(&cfg);
    }

    clock_gettime(CLOCK_MONOTONIC, &total_end);
    double total_time =
        calculate_time_diff_ns(&total_start, &total_end) /
        (double)NANOS_PER_SECOND;

    printf("\n");
    printf("========================================\n");
    printf("  所有测试完成! 总耗时: %.2f 秒\n", total_time);
    printf("========================================\n");

    return 0;
}
