/*
    fstest - 文件系统综合测试工具
    单个可执行程序，通过选项来选择功能

    使用方法：
    ./fstest -d <测试目录> [-m <模式>] [-j <线程数>] [-s <IO大小>]
             [-f <文件大小MB>] [-i <迭代次数>] [-v]

    测试模式 (-m):
      0 或 all : 运行所有测试 (默认)
      1        : 功能正确性测试
      2        : 数据一致性测试
      3        : 异常场景测试
      4        : 并发测试
      5        : 压力和稳定性测试
      6        : 性能测试

    示例：
      ./fstest -d /tmp/fstest_data -m 0          # 运行所有测试
      ./fstest -d /mnt/nufs -m 6 -j 4 -s 4096   # 性能测试，4线程
      ./fstest -d /tmp/fstest_data -m 1           # 仅功能正确性测试
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
    printf("  -m <mode>    测试模式 (默认: 0)\n");
    printf("                 0 = 运行所有测试\n");
    printf("                 1 = 功能正确性测试\n");
    printf("                 2 = 数据一致性测试\n");
    printf("                 3 = 异常场景测试\n");
    printf("                 4 = 并发测试\n");
    printf("                 5 = 压力和稳定性测试\n");
    printf("                 6 = 性能测试\n");
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
    printf("  %s -d /mnt/nufs -m 6 -j 4 -s 4096\n", prog);
    printf("  %s -d /tmp/fstest_data -m 1\n", prog);
}

static const char *mode_name(int mode) {
    switch (mode) {
        case 0: return "所有测试";
        case 1: return "功能正确性测试";
        case 2: return "数据一致性测试";
        case 3: return "异常场景测试";
        case 4: return "并发测试";
        case 5: return "压力和稳定性测试";
        case 6: return "性能测试";
        default: return "未知";
    }
}

int main(int argc, char *argv[]) {
    struct fstest_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.jobs = DEFAULT_JOBS;
    cfg.io_size = DEFAULT_IO_SIZE;
    cfg.file_size = DEFAULT_FILE_SIZE;
    cfg.iter_count = DEFAULT_ITER;
    cfg.test_mode = 0;
    cfg.verbose = 0;

    int opt;
    while ((opt = getopt(argc, argv, "d:m:j:s:f:i:vh")) != -1) {
        switch (opt) {
            case 'd':
                strncpy(cfg.dir, optarg, MAX_PATH_LEN - 1);
                break;
            case 'm':
                cfg.test_mode = atoi(optarg);
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

    if (cfg.test_mode < 0 || cfg.test_mode > 6) {
        fprintf(stderr, "Error: 无效的测试模式 %d (有效范围: 0-6)\n",
                cfg.test_mode);
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
    printf("  测试模式:   %d (%s)\n", cfg.test_mode,
           mode_name(cfg.test_mode));
    printf("  线程数:     %d\n", cfg.jobs);
    printf("  IO 大小:    %zu bytes\n", cfg.io_size);
    printf("  文件大小:   %zu MB\n", cfg.file_size / _1MB_BYTES);
    printf("  迭代次数:   %d\n", cfg.iter_count);

    srand(time(NULL));

    /* 根据模式运行对应测试 */
    struct timespec total_start, total_end;
    clock_gettime(CLOCK_MONOTONIC, &total_start);

    if (cfg.test_mode == 0 || cfg.test_mode == 1) {
        run_functional_tests(&cfg);
    }
    if (cfg.test_mode == 0 || cfg.test_mode == 2) {
        run_consistency_tests(&cfg);
    }
    if (cfg.test_mode == 0 || cfg.test_mode == 3) {
        run_exception_tests(&cfg);
    }
    if (cfg.test_mode == 0 || cfg.test_mode == 4) {
        run_concurrent_tests(&cfg);
    }
    if (cfg.test_mode == 0 || cfg.test_mode == 5) {
        run_stress_tests(&cfg);
    }
    if (cfg.test_mode == 0 || cfg.test_mode == 6) {
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
