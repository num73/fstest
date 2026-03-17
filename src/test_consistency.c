/*
    数据一致性测试模块实现
    测试项目：
    - 写后读校验
    - 校验和/hash 比对
    - 随机读写一致性检查
    - 大文件、小文件、空文件测试
    - 稀疏文件测试
    - 反复覆盖写后的结果验证
*/

#include "test_consistency.h"

#include <sys/stat.h>

/* 简单CRC32校验和 (用于数据一致性校验) */
static uint32_t crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        if (crc & 1)
            crc = (crc >> 1) ^ 0xEDB88320;
        else
            crc >>= 1;
    }
    return crc;
}

static uint32_t compute_crc32(const void *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_byte(crc, p[i]);
    }
    return crc ^ 0xFFFFFFFF;
}

/* 测试：写后读校验 */
static void test_write_read_verify(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "consist_write_read.dat");

    size_t data_size = 64 * _1KB_BYTES;
    char *wbuf = malloc(data_size);
    char *rbuf = malloc(data_size);
    if (!wbuf || !rbuf) {
        TEST_FAIL("write-read verify", "malloc failed");
        free(wbuf);
        free(rbuf);
        return;
    }

    /* 写入随机数据 */
    srand(42);
    fill_rand_buffer(wbuf, data_size);

    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("write-read verify", strerror(errno));
        free(wbuf);
        free(rbuf);
        return;
    }
    ssize_t w = write(fd, wbuf, data_size);
    if (w != (ssize_t)data_size) {
        TEST_FAIL("write-read verify", "write incomplete");
        close(fd);
        free(wbuf);
        free(rbuf);
        unlink(path);
        return;
    }
    close(fd);

    /* 读回数据 */
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        TEST_FAIL("write-read verify", strerror(errno));
        free(wbuf);
        free(rbuf);
        unlink(path);
        return;
    }
    ssize_t r = read(fd, rbuf, data_size);
    close(fd);
    if (r != (ssize_t)data_size) {
        TEST_FAIL("write-read verify", "read incomplete");
        free(wbuf);
        free(rbuf);
        unlink(path);
        return;
    }

    /* 逐字节比对 */
    if (memcmp(wbuf, rbuf, data_size) != 0) {
        TEST_FAIL("write-read verify", "data mismatch");
    } else {
        TEST_PASS("write-read verify (64KB)");
    }

    free(wbuf);
    free(rbuf);
    unlink(path);
}

/* 测试：校验和/hash比对 */
static void test_checksum_verify(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "consist_checksum.dat");

    size_t data_size = 128 * _1KB_BYTES;
    char *buf = malloc(data_size);
    if (!buf) {
        TEST_FAIL("checksum verify", "malloc failed");
        return;
    }

    srand(123);
    fill_rand_buffer(buf, data_size);
    uint32_t write_crc = compute_crc32(buf, data_size);

    /* 写入 */
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("checksum verify", strerror(errno));
        free(buf);
        return;
    }
    (void)!write(fd, buf, data_size);
    close(fd);

    /* 清空buffer后读回 */
    memset(buf, 0, data_size);
    fd = open(path, O_RDONLY);
    (void)!read(fd, buf, data_size);
    close(fd);

    uint32_t read_crc = compute_crc32(buf, data_size);

    if (write_crc == read_crc) {
        TEST_PASS("checksum/CRC32 verify (128KB)");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "CRC mismatch: 0x%08X vs 0x%08X",
                 write_crc, read_crc);
        TEST_FAIL("checksum verify", msg);
    }

    free(buf);
    unlink(path);
}

/* 测试：随机读写一致性检查 */
static void test_random_rw_consistency(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "consist_random_rw.dat");

    size_t file_size = 256 * _1KB_BYTES;
    size_t block_size = 4 * _1KB_BYTES;
    size_t block_count = file_size / block_size;

    /* 创建文件并填充已知模式 */
    char *block = malloc(block_size);
    if (!block) {
        TEST_FAIL("random rw consistency", "malloc failed");
        return;
    }

    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("random rw consistency", strerror(errno));
        free(block);
        return;
    }

    /* 每个块用块号填充 */
    for (size_t i = 0; i < block_count; i++) {
        memset(block, (int)(i & 0xFF), block_size);
        (void)!write(fd, block, block_size);
    }

    /* 随机选择若干块写入新数据，记录写入内容 */
    int num_updates = 20;
    size_t *updated_blocks = malloc(num_updates * sizeof(size_t));
    char *updated_data = malloc(num_updates * block_size);
    srand(99);

    for (int i = 0; i < num_updates; i++) {
        size_t blk = rand() % block_count;
        updated_blocks[i] = blk;
        fill_rand_buffer(updated_data + i * block_size, block_size);
        lseek(fd, blk * block_size, SEEK_SET);
        (void)!write(fd, updated_data + i * block_size, block_size);
    }

    /* 验证最后一次更新的块内容 */
    int pass = 1;
    for (int i = num_updates - 1; i >= 0; i--) {
        size_t blk = updated_blocks[i];
        /* 检查这个块是否被后续更新覆盖 */
        int overwritten = 0;
        for (int j = i + 1; j < num_updates; j++) {
            if (updated_blocks[j] == blk) {
                overwritten = 1;
                break;
            }
        }
        if (overwritten) continue;

        lseek(fd, blk * block_size, SEEK_SET);
        memset(block, 0, block_size);
        (void)!read(fd, block, block_size);
        if (memcmp(block, updated_data + i * block_size, block_size) != 0) {
            pass = 0;
            break;
        }
    }
    close(fd);

    if (pass) {
        TEST_PASS("random read/write consistency");
    } else {
        TEST_FAIL("random rw consistency",
                  "data mismatch after random updates");
    }

    free(block);
    free(updated_blocks);
    free(updated_data);
    unlink(path);
}

/* 测试：大文件、小文件、空文件 */
static void test_various_file_sizes(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];

    /* 空文件 */
    make_test_path(path, sizeof(path), cfg->dir, "consist_empty.dat");
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("empty file", strerror(errno));
        return;
    }
    close(fd);
    struct stat st;
    stat(path, &st);
    if (st.st_size != 0) {
        TEST_FAIL("empty file", "size not zero");
    } else {
        TEST_PASS("empty file (0 bytes)");
    }
    unlink(path);

    /* 小文件 (1 byte) */
    make_test_path(path, sizeof(path), cfg->dir, "consist_tiny.dat");
    fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    (void)!write(fd, "X", 1);
    close(fd);
    fd = open(path, O_RDONLY);
    char c = 0;
    (void)!read(fd, &c, 1);
    close(fd);
    if (c == 'X') {
        TEST_PASS("tiny file (1 byte)");
    } else {
        TEST_FAIL("tiny file", "content mismatch");
    }
    unlink(path);

    /* 较大文件 (使用配置的 file_size, 至少 1MB) */
    size_t large_size = cfg->file_size > _1MB_BYTES ? cfg->file_size : _1MB_BYTES;
    /* 限制测试大小避免过长 */
    if (large_size > 64 * _1MB_BYTES) {
        large_size = 64 * _1MB_BYTES;
    }
    make_test_path(path, sizeof(path), cfg->dir, "consist_large.dat");
    fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("large file", strerror(errno));
        return;
    }

    size_t block_size = 4 * _1MB_BYTES;
    char *buf = malloc(block_size);
    if (!buf) {
        TEST_FAIL("large file", "malloc failed");
        close(fd);
        return;
    }

    srand(777);
    uint32_t write_crc = 0xFFFFFFFF;
    size_t written = 0;
    while (written < large_size) {
        size_t to_write = block_size;
        if (written + to_write > large_size) {
            to_write = large_size - written;
        }
        fill_rand_buffer(buf, to_write);
        /* 增量计算CRC */
        const uint8_t *p = (const uint8_t *)buf;
        for (size_t i = 0; i < to_write; i++) {
            write_crc = crc32_byte(write_crc, p[i]);
        }
        ssize_t w = write(fd, buf, to_write);
        if (w != (ssize_t)to_write) {
            TEST_FAIL("large file", "write incomplete");
            close(fd);
            free(buf);
            unlink(path);
            return;
        }
        written += to_write;
    }
    write_crc ^= 0xFFFFFFFF;
    close(fd);

    /* 读回校验 */
    fd = open(path, O_RDONLY);
    uint32_t read_crc = 0xFFFFFFFF;
    size_t total_read = 0;
    while (total_read < large_size) {
        size_t to_read = block_size;
        if (total_read + to_read > large_size) {
            to_read = large_size - total_read;
        }
        ssize_t r = read(fd, buf, to_read);
        if (r <= 0) break;
        const uint8_t *p = (const uint8_t *)buf;
        for (size_t i = 0; i < (size_t)r; i++) {
            read_crc = crc32_byte(read_crc, p[i]);
        }
        total_read += r;
    }
    read_crc ^= 0xFFFFFFFF;
    close(fd);

    if (write_crc == read_crc && total_read == large_size) {
        char msg[64];
        snprintf(msg, sizeof(msg), "large file (%zu MB)",
                 large_size / _1MB_BYTES);
        TEST_PASS(msg);
    } else {
        TEST_FAIL("large file", "CRC mismatch or size mismatch");
    }

    free(buf);
    unlink(path);
}

/* 测试：稀疏文件 */
static void test_sparse_file(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "consist_sparse.dat");

    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("sparse file", strerror(errno));
        return;
    }

    /* 在偏移 1MB 处写入数据 */
    off_t sparse_offset = 1 * _1MB_BYTES;
    const char *data = "sparse data here";
    size_t data_len = strlen(data);
    lseek(fd, sparse_offset, SEEK_SET);
    (void)!write(fd, data, data_len);
    close(fd);

    /* 验证文件大小 */
    struct stat st;
    stat(path, &st);
    if (st.st_size != (off_t)(sparse_offset + data_len)) {
        TEST_FAIL("sparse file", "size mismatch");
        unlink(path);
        return;
    }

    /* 读取空洞区域应该是0 */
    fd = open(path, O_RDONLY);
    char buf[4096] = {0};
    ssize_t r = read(fd, buf, sizeof(buf));
    if (r <= 0) {
        TEST_FAIL("sparse file", "read failed");
        close(fd);
        unlink(path);
        return;
    }
    int all_zero = 1;
    for (ssize_t i = 0; i < r; i++) {
        if (buf[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    if (!all_zero) {
        TEST_FAIL("sparse file", "hole region not zero");
        close(fd);
        unlink(path);
        return;
    }

    /* 读取写入的数据 */
    lseek(fd, sparse_offset, SEEK_SET);
    memset(buf, 0, sizeof(buf));
    r = read(fd, buf, data_len);
    close(fd);
    if (r == (ssize_t)data_len && memcmp(buf, data, data_len) == 0) {
        TEST_PASS("sparse file (hole + data)");
    } else {
        TEST_FAIL("sparse file", "data mismatch in non-hole region");
    }

    /* 检查实际占用空间是否小于逻辑大小（稀疏文件特征） */
    if (st.st_blocks * 512 < st.st_size) {
        TEST_PASS("sparse file (actual blocks < logical size)");
    } else {
        TEST_SKIP("sparse file blocks check",
                  "filesystem may not support sparse files");
    }

    unlink(path);
}

/* 测试：反复覆盖写后的结果验证 */
static void test_repeated_overwrite(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "consist_overwrite.dat");

    size_t data_size = 16 * _1KB_BYTES;
    char *wbuf = malloc(data_size);
    char *rbuf = malloc(data_size);
    if (!wbuf || !rbuf) {
        TEST_FAIL("repeated overwrite", "malloc failed");
        free(wbuf);
        free(rbuf);
        return;
    }

    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("repeated overwrite", strerror(errno));
        free(wbuf);
        free(rbuf);
        return;
    }

    /* 写入初始数据 */
    fill_rand_buffer(wbuf, data_size);
    (void)!write(fd, wbuf, data_size);

    /* 反复覆盖 50 次 */
    int pass = 1;
    int overwrite_count = 50;
    for (int i = 0; i < overwrite_count; i++) {
        /* 每次生成新的随机数据 */
        srand(i * 31 + 7);
        fill_rand_buffer(wbuf, data_size);

        lseek(fd, 0, SEEK_SET);
        ssize_t w = write(fd, wbuf, data_size);
        if (w != (ssize_t)data_size) {
            pass = 0;
            break;
        }

        /* 立即读回验证 */
        lseek(fd, 0, SEEK_SET);
        ssize_t r = read(fd, rbuf, data_size);
        if (r != (ssize_t)data_size ||
            memcmp(wbuf, rbuf, data_size) != 0) {
            pass = 0;
            break;
        }
    }
    close(fd);

    if (pass) {
        char msg[64];
        snprintf(msg, sizeof(msg),
                 "repeated overwrite (%d iterations)", overwrite_count);
        TEST_PASS(msg);
    } else {
        TEST_FAIL("repeated overwrite",
                  "data mismatch after overwrite");
    }

    free(wbuf);
    free(rbuf);
    unlink(path);
}

void run_consistency_tests(const struct fstest_config *cfg) {
    printf("\n");
    printf("========================================\n");
    printf("  2. 数据一致性测试 (Consistency Tests)\n");
    printf("========================================\n");

    test_write_read_verify(cfg);
    test_checksum_verify(cfg);
    test_random_rw_consistency(cfg);
    test_various_file_sizes(cfg);
    test_sparse_file(cfg);
    test_repeated_overwrite(cfg);

    printf("--- 数据一致性测试完成 ---\n");
}
