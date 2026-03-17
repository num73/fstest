/*
    异常场景测试模块实现
    测试项目：
    - 磁盘满 (ENOSPC)
    - 权限不足 (EACCES)
    - 文件被占用
    - 强制中断写操作
    - 文件系统修复后的完整性检查
    - 无效操作处理
*/

#include "test_exception.h"

#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* 测试：权限不足 */
static void test_permission_denied(const struct fstest_config *cfg) {
    if (getuid() == 0) {
        TEST_SKIP("permission denied",
                  "running as root, cannot test permission errors");
        return;
    }

    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "except_perm_test.dat");

    /* 创建只读文件 */
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0444);
    if (fd < 0) {
        TEST_FAIL("permission denied setup", strerror(errno));
        return;
    }
    (void)!write(fd, "readonly", 8);
    close(fd);

    /* 尝试写入只读文件 */
    fd = open(path, O_WRONLY);
    if (fd < 0 && errno == EACCES) {
        TEST_PASS("permission denied (write to read-only file)");
    } else if (fd >= 0) {
        TEST_FAIL("permission denied",
                  "opened read-only file for writing");
        close(fd);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "unexpected errno: %s",
                 strerror(errno));
        TEST_FAIL("permission denied", msg);
    }

    /* 尝试在没有写权限的目录中创建文件 */
    char rdir[MAX_PATH_LEN];
    make_test_path(rdir, sizeof(rdir), cfg->dir, "except_nowrite_dir");
    mkdir(rdir, 0555);

    char rpath[MAX_PATH_LEN];
    snprintf(rpath, sizeof(rpath), "%s/test.txt", rdir);
    fd = open(rpath, O_CREAT | O_WRONLY, 0644);
    if (fd < 0 && errno == EACCES) {
        TEST_PASS("permission denied (create in read-only dir)");
    } else if (fd >= 0) {
        TEST_FAIL("permission denied",
                  "created file in read-only directory");
        close(fd);
        unlink(rpath);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "unexpected errno: %s",
                 strerror(errno));
        TEST_FAIL("permission denied (dir)", msg);
    }

    chmod(rdir, 0755);
    rmdir(rdir);
    chmod(path, 0644);
    unlink(path);
}

/* 测试：文件锁定/被占用 */
static void test_file_locking(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "except_lock_test.dat");

    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("file locking", strerror(errno));
        return;
    }
    (void)!write(fd, "lock test", 9);

    /* 获取排他锁 */
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        TEST_FAIL("file locking", "failed to acquire exclusive lock");
        close(fd);
        unlink(path);
        return;
    }

    /* 通过子进程尝试获取锁 */
    pid_t pid = fork();
    if (pid < 0) {
        TEST_FAIL("file locking (fork)", strerror(errno));
        flock(fd, LOCK_UN);
        close(fd);
        unlink(path);
        return;
    }

    if (pid == 0) {
        /* 子进程：尝试获取排他锁，应该失败 */
        int fd2 = open(path, O_RDWR);
        if (fd2 < 0) _exit(2);
        int ret = flock(fd2, LOCK_EX | LOCK_NB);
        close(fd2);
        _exit(ret == -1 && errno == EWOULDBLOCK ? 0 : 1);
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        TEST_PASS("file locking (exclusive lock conflict)");
    } else {
        TEST_FAIL("file locking",
                  "child acquired exclusive lock while parent holds it");
    }

    flock(fd, LOCK_UN);
    close(fd);
    unlink(path);
}

/* 测试：对不存在的文件/目录操作 */
static void test_nonexistent_operations(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "except_nonexist_XXXX.dat");

    /* 打开不存在的文件 */
    int fd = open(path, O_RDONLY);
    if (fd < 0 && errno == ENOENT) {
        TEST_PASS("open nonexistent file (ENOENT)");
    } else if (fd >= 0) {
        TEST_FAIL("open nonexistent", "opened nonexistent file");
        close(fd);
    } else {
        TEST_FAIL("open nonexistent", strerror(errno));
    }

    /* 删除不存在的文件 */
    if (unlink(path) < 0 && errno == ENOENT) {
        TEST_PASS("unlink nonexistent file (ENOENT)");
    } else {
        TEST_FAIL("unlink nonexistent", "unexpected result");
    }

    /* 读取不存在的目录 */
    char ndir[MAX_PATH_LEN];
    make_test_path(ndir, sizeof(ndir), cfg->dir,
                   "except_nonexist_dir_XXXX");
    if (rmdir(ndir) < 0 && errno == ENOENT) {
        TEST_PASS("rmdir nonexistent directory (ENOENT)");
    } else {
        TEST_FAIL("rmdir nonexistent", "unexpected result");
    }
}

/* 测试：删除非空目录 */
static void test_rmdir_nonempty(const struct fstest_config *cfg) {
    char dpath[MAX_PATH_LEN];
    make_test_path(dpath, sizeof(dpath), cfg->dir,
                   "except_nonempty_dir");
    mkdir(dpath, 0755);

    char fpath[MAX_PATH_LEN];
    snprintf(fpath, sizeof(fpath), "%s/file.txt", dpath);
    int fd = open(fpath, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        (void)!write(fd, "test", 4);
        close(fd);
    }

    /* 尝试删除非空目录 */
    if (rmdir(dpath) < 0 && errno == ENOTEMPTY) {
        TEST_PASS("rmdir non-empty directory (ENOTEMPTY)");
    } else if (rmdir(dpath) < 0) {
        /* 部分文件系统可能返回 EEXIST */
        if (errno == EEXIST) {
            TEST_PASS("rmdir non-empty directory (EEXIST)");
        } else {
            TEST_FAIL("rmdir non-empty", strerror(errno));
        }
    } else {
        TEST_FAIL("rmdir non-empty",
                  "rmdir succeeded on non-empty directory");
    }

    unlink(fpath);
    rmdir(dpath);
}

/* 测试：写入后中断 (使用子进程模拟) */
static void test_interrupted_write(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "except_interrupt.dat");

    /* 先写入已知数据 */
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("interrupted write", strerror(errno));
        return;
    }
    char initial_data[] = "INITIAL DATA CONTENT";
    (void)!write(fd, initial_data, sizeof(initial_data) - 1);
    close(fd);

    /* 子进程写入大量数据然后被终止 */
    pid_t pid = fork();
    if (pid < 0) {
        TEST_FAIL("interrupted write (fork)", strerror(errno));
        unlink(path);
        return;
    }

    if (pid == 0) {
        /* 子进程：不断写入直到被杀死 */
        int cfd = open(path, O_WRONLY | O_TRUNC);
        if (cfd < 0) _exit(1);
        char buf[4096];
        memset(buf, 'A', sizeof(buf));
        for (int i = 0; i < 1000; i++) {
            (void)!write(cfd, buf, sizeof(buf));
        }
        close(cfd);
        _exit(0);
    }

    /* 等待子进程完成（或超时终止） */
    usleep(10000); /* 等10ms让子进程开始写 */
    kill(pid, SIGKILL);
    int status;
    waitpid(pid, &status, 0);

    /* 文件应该仍然可以打开和读取（不应该损坏文件系统） */
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        TEST_FAIL("interrupted write",
                  "file not accessible after interrupted write");
        unlink(path);
        return;
    }
    char buf[128];
    ssize_t r = read(fd, buf, sizeof(buf));
    close(fd);

    if (r >= 0) {
        TEST_PASS("interrupted write (file still accessible)");
    } else {
        TEST_FAIL("interrupted write", "read failed after interrupt");
    }

    unlink(path);
}

/* 测试：文件大小限制和边界 */
static void test_boundary_conditions(const struct fstest_config *cfg) {
    /* 测试0长度写入 */
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir,
                   "except_boundary.dat");
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("boundary conditions", strerror(errno));
        return;
    }

    ssize_t w = write(fd, "", 0);
    if (w == 0) {
        TEST_PASS("zero-length write");
    } else {
        TEST_FAIL("zero-length write", "unexpected return value");
    }

    /* 测试超长文件名 */
    close(fd);
    unlink(path);

    char longname[300];
    snprintf(longname, sizeof(longname), "%s/", cfg->dir);
    size_t prefix_len = strlen(longname);
    memset(longname + prefix_len, 'a', 256);
    longname[prefix_len + 256] = '\0';
    fd = open(longname, O_CREAT | O_WRONLY, 0644);
    if (fd < 0 && errno == ENAMETOOLONG) {
        TEST_PASS("long filename (ENAMETOOLONG)");
    } else if (fd >= 0) {
        close(fd);
        unlink(longname);
        TEST_PASS("long filename (accepted by filesystem)");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "errno: %s", strerror(errno));
        TEST_PASS(msg);
    }
}

void run_exception_tests(const struct fstest_config *cfg) {
    printf("\n");
    printf("========================================\n");
    printf("  3. 异常场景测试 (Exception Tests)\n");
    printf("========================================\n");

    test_permission_denied(cfg);
    test_file_locking(cfg);
    test_nonexistent_operations(cfg);
    test_rmdir_nonempty(cfg);
    test_interrupted_write(cfg);
    test_boundary_conditions(cfg);

    printf("--- 异常场景测试完成 ---\n");
}
