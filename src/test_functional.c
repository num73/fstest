/*
    功能正确性测试模块实现
    测试项目：
    - 创建/删除文件
    - 读/写/追加/截断
    - 创建/删除目录
    - 重命名、移动
    - 权限、时间戳、属性检查
    - 硬链接/软链接
    - 路径解析（相对路径、绝对路径、.、..）
*/

#include "test_functional.h"

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

/* 测试：创建和删除文件 */
static void test_create_delete_file(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "func_create_test.dat");

    /* 创建文件 */
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("create file", strerror(errno));
        return;
    }
    close(fd);

    /* 验证文件存在 */
    if (access(path, F_OK) != 0) {
        TEST_FAIL("create file", "file not found after creation");
        return;
    }

    /* 删除文件 */
    if (unlink(path) != 0) {
        TEST_FAIL("delete file", strerror(errno));
        return;
    }

    /* 验证文件已删除 */
    if (access(path, F_OK) == 0) {
        TEST_FAIL("delete file", "file still exists after unlink");
        return;
    }

    TEST_PASS("create/delete file");
}

/* 测试：读/写/追加/截断 */
static void test_read_write_append_truncate(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "func_rw_test.dat");

    /* 写入数据 */
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("write file", strerror(errno));
        return;
    }
    const char *data1 = "Hello, fstest!";
    ssize_t w = write(fd, data1, strlen(data1));
    if (w != (ssize_t)strlen(data1)) {
        TEST_FAIL("write file", "write returned unexpected count");
        close(fd);
        unlink(path);
        return;
    }
    close(fd);

    /* 读取并验证 */
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        TEST_FAIL("read file", strerror(errno));
        unlink(path);
        return;
    }
    char rbuf[128] = {0};
    ssize_t r = read(fd, rbuf, sizeof(rbuf) - 1);
    close(fd);
    if (r != (ssize_t)strlen(data1) || strcmp(rbuf, data1) != 0) {
        TEST_FAIL("read file", "data mismatch after write");
        unlink(path);
        return;
    }
    TEST_PASS("write and read file");

    /* 追加数据 */
    fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) {
        TEST_FAIL("append file", strerror(errno));
        unlink(path);
        return;
    }
    const char *data2 = " Appended.";
    w = write(fd, data2, strlen(data2));
    close(fd);
    if (w != (ssize_t)strlen(data2)) {
        TEST_FAIL("append file", "append write returned unexpected count");
        unlink(path);
        return;
    }

    /* 验证追加内容 */
    fd = open(path, O_RDONLY);
    memset(rbuf, 0, sizeof(rbuf));
    r = read(fd, rbuf, sizeof(rbuf) - 1);
    close(fd);
    size_t expected_len = strlen(data1) + strlen(data2);
    if (r != (ssize_t)expected_len) {
        TEST_FAIL("append file",
                  "file size mismatch after append");
        unlink(path);
        return;
    }
    TEST_PASS("append file");

    /* 截断文件 */
    if (truncate(path, 5) != 0) {
        TEST_FAIL("truncate file", strerror(errno));
        unlink(path);
        return;
    }
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size != 5) {
        TEST_FAIL("truncate file", "size mismatch after truncate");
        unlink(path);
        return;
    }

    /* 验证截断后内容 */
    fd = open(path, O_RDONLY);
    memset(rbuf, 0, sizeof(rbuf));
    r = read(fd, rbuf, sizeof(rbuf) - 1);
    close(fd);
    if (r != 5 || strncmp(rbuf, "Hello", 5) != 0) {
        TEST_FAIL("truncate file", "content mismatch after truncate");
        unlink(path);
        return;
    }
    TEST_PASS("truncate file");

    unlink(path);
}

/* 测试：创建和删除目录 */
static void test_create_delete_directory(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "func_dir_test");

    /* 创建目录 */
    if (mkdir(path, 0755) != 0) {
        TEST_FAIL("create directory", strerror(errno));
        return;
    }

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        TEST_FAIL("create directory", "not a directory after mkdir");
        rmdir(path);
        return;
    }

    /* 在目录内创建文件 */
    char subfile[MAX_PATH_LEN];
    snprintf(subfile, sizeof(subfile), "%s/test.txt", path);
    int fd = open(subfile, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        TEST_FAIL("create file in directory", strerror(errno));
        rmdir(path);
        return;
    }
    (void)!write(fd, "test", 4);
    close(fd);

    /* 删除文件后删除目录 */
    unlink(subfile);
    if (rmdir(path) != 0) {
        TEST_FAIL("delete directory", strerror(errno));
        return;
    }
    if (access(path, F_OK) == 0) {
        TEST_FAIL("delete directory", "dir still exists after rmdir");
        return;
    }
    TEST_PASS("create/delete directory");
}

/* 测试：重命名和移动 */
static void test_rename_move(const struct fstest_config *cfg) {
    char path1[MAX_PATH_LEN], path2[MAX_PATH_LEN];
    make_test_path(path1, sizeof(path1), cfg->dir, "func_rename_src.dat");
    make_test_path(path2, sizeof(path2), cfg->dir, "func_rename_dst.dat");

    /* 创建源文件 */
    int fd = open(path1, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("rename", strerror(errno));
        return;
    }
    (void)!write(fd, "rename test", 11);
    close(fd);

    /* 重命名 */
    if (rename(path1, path2) != 0) {
        TEST_FAIL("rename file", strerror(errno));
        unlink(path1);
        return;
    }
    if (access(path1, F_OK) == 0) {
        TEST_FAIL("rename file", "source still exists");
        unlink(path1);
        unlink(path2);
        return;
    }

    /* 验证目标文件内容 */
    fd = open(path2, O_RDONLY);
    char buf[64] = {0};
    (void)!read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (strcmp(buf, "rename test") != 0) {
        TEST_FAIL("rename file", "content mismatch after rename");
        unlink(path2);
        return;
    }
    TEST_PASS("rename/move file");

    /* 测试跨目录移动 */
    char subdir[MAX_PATH_LEN];
    make_test_path(subdir, sizeof(subdir), cfg->dir, "func_move_dir");
    mkdir(subdir, 0755);
    char path3[MAX_PATH_LEN];
    snprintf(path3, sizeof(path3), "%s/moved.dat", subdir);
    if (rename(path2, path3) != 0) {
        TEST_FAIL("move file across dirs", strerror(errno));
        unlink(path2);
        rmdir(subdir);
        return;
    }
    if (access(path3, F_OK) != 0) {
        TEST_FAIL("move file across dirs", "file not found at destination");
        rmdir(subdir);
        return;
    }
    TEST_PASS("move file across directories");
    unlink(path3);
    rmdir(subdir);
}

/* 测试：权限、时间戳、属性 */
static void test_permissions_timestamps(const struct fstest_config *cfg) {
    char path[MAX_PATH_LEN];
    make_test_path(path, sizeof(path), cfg->dir, "func_perm_test.dat");

    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("permissions", strerror(errno));
        return;
    }
    (void)!write(fd, "perm", 4);
    close(fd);

    /* 修改权限 */
    if (chmod(path, 0444) != 0) {
        TEST_FAIL("chmod", strerror(errno));
        unlink(path);
        return;
    }
    struct stat st;
    stat(path, &st);
    if ((st.st_mode & 0777) != 0444) {
        TEST_FAIL("chmod", "permission mismatch after chmod");
        chmod(path, 0644);
        unlink(path);
        return;
    }

    /* 只读文件不应该可以写入（非root用户） */
    if (getuid() != 0) {
        fd = open(path, O_WRONLY);
        if (fd >= 0) {
            TEST_FAIL("permission enforcement",
                      "writable after chmod 0444");
            close(fd);
        } else {
            TEST_PASS("permission enforcement (read-only)");
        }
    } else {
        TEST_SKIP("permission enforcement",
                  "running as root, cannot test");
    }
    TEST_PASS("chmod permissions");

    /* 时间戳检查 */
    struct timespec ts[2];
    ts[0].tv_sec = 1000000;
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = 2000000;
    ts[1].tv_nsec = 0;
    chmod(path, 0644);
    if (utimensat(AT_FDCWD, path, ts, 0) != 0) {
        TEST_FAIL("set timestamps", strerror(errno));
        unlink(path);
        return;
    }
    stat(path, &st);
    if (st.st_atime != 1000000 || st.st_mtime != 2000000) {
        TEST_FAIL("timestamps", "timestamp mismatch after utimensat");
        unlink(path);
        return;
    }
    TEST_PASS("timestamps (atime/mtime)");

    unlink(path);
}

/* 测试：硬链接和软链接 */
static void test_links(const struct fstest_config *cfg) {
    char orig[MAX_PATH_LEN], hlink[MAX_PATH_LEN], slink[MAX_PATH_LEN];
    make_test_path(orig, sizeof(orig), cfg->dir, "func_link_orig.dat");
    make_test_path(hlink, sizeof(hlink), cfg->dir, "func_link_hard.dat");
    make_test_path(slink, sizeof(slink), cfg->dir, "func_link_soft.dat");

    /* 创建原始文件 */
    int fd = open(orig, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("links", strerror(errno));
        return;
    }
    (void)!write(fd, "link test data", 14);
    close(fd);

    /* 硬链接 */
    if (link(orig, hlink) != 0) {
        TEST_FAIL("hard link", strerror(errno));
        unlink(orig);
        return;
    }
    struct stat st1, st2;
    stat(orig, &st1);
    stat(hlink, &st2);
    if (st1.st_ino != st2.st_ino) {
        TEST_FAIL("hard link", "inode mismatch");
        unlink(hlink);
        unlink(orig);
        return;
    }
    if (st1.st_nlink != 2) {
        TEST_FAIL("hard link", "nlink != 2");
        unlink(hlink);
        unlink(orig);
        return;
    }

    /* 通过硬链接读取内容 */
    fd = open(hlink, O_RDONLY);
    char buf[64] = {0};
    (void)!read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (strcmp(buf, "link test data") != 0) {
        TEST_FAIL("hard link", "content mismatch through hard link");
        unlink(hlink);
        unlink(orig);
        return;
    }
    TEST_PASS("hard link");
    unlink(hlink);

    /* 软链接 */
    if (symlink(orig, slink) != 0) {
        TEST_FAIL("soft link", strerror(errno));
        unlink(orig);
        return;
    }
    char target[MAX_PATH_LEN] = {0};
    ssize_t len = readlink(slink, target, sizeof(target) - 1);
    if (len < 0 || strcmp(target, orig) != 0) {
        TEST_FAIL("soft link", "readlink target mismatch");
        unlink(slink);
        unlink(orig);
        return;
    }

    /* 通过软链接读取内容 */
    fd = open(slink, O_RDONLY);
    memset(buf, 0, sizeof(buf));
    (void)!read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (strcmp(buf, "link test data") != 0) {
        TEST_FAIL("soft link", "content mismatch through symlink");
        unlink(slink);
        unlink(orig);
        return;
    }

    /* 删除源文件后软链接应该失效 */
    unlink(orig);
    if (access(slink, F_OK) == 0) {
        /* lstat 应该成功但打开应该失败 */
        fd = open(slink, O_RDONLY);
        if (fd >= 0) {
            TEST_FAIL("soft link (dangling)",
                      "opened dangling symlink");
            close(fd);
        } else {
            TEST_PASS("soft link (dangling detection)");
        }
    } else {
        TEST_PASS("soft link (dangling detection)");
    }
    TEST_PASS("soft link");
    unlink(slink);
}

/* 测试：路径解析 */
static void test_path_resolution(const struct fstest_config *cfg) {
    char subdir1[MAX_PATH_LEN], subdir2[MAX_PATH_LEN];
    make_test_path(subdir1, sizeof(subdir1), cfg->dir, "func_path_a");
    snprintf(subdir2, sizeof(subdir2), "%s/func_path_a/sub_b", cfg->dir);

    mkdir(subdir1, 0755);
    mkdir(subdir2, 0755);

    /* 在深层目录创建文件 */
    char fpath[MAX_PATH_LEN];
    snprintf(fpath, sizeof(fpath), "%s/test.txt", subdir2);
    int fd = open(fpath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        TEST_FAIL("path resolution", strerror(errno));
        remove_dir_recursive(subdir1);
        return;
    }
    (void)!write(fd, "path_test", 9);
    close(fd);

    /* 使用 .. 路径引用 */
    char dotdot_path[MAX_PATH_LEN];
    snprintf(dotdot_path, sizeof(dotdot_path),
             "%s/func_path_a/sub_b/../sub_b/test.txt", cfg->dir);
    fd = open(dotdot_path, O_RDONLY);
    if (fd < 0) {
        TEST_FAIL("path resolution (..)", strerror(errno));
        remove_dir_recursive(subdir1);
        return;
    }
    char buf[64] = {0};
    (void)!read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (strcmp(buf, "path_test") != 0) {
        TEST_FAIL("path resolution (..)", "content mismatch");
        remove_dir_recursive(subdir1);
        return;
    }
    TEST_PASS("path resolution (.. parent directory)");

    /* 使用 . 路径引用 */
    char dot_path[MAX_PATH_LEN];
    snprintf(dot_path, sizeof(dot_path),
             "%s/func_path_a/./sub_b/test.txt", cfg->dir);
    fd = open(dot_path, O_RDONLY);
    if (fd < 0) {
        TEST_FAIL("path resolution (.)", strerror(errno));
        remove_dir_recursive(subdir1);
        return;
    }
    memset(buf, 0, sizeof(buf));
    (void)!read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (strcmp(buf, "path_test") != 0) {
        TEST_FAIL("path resolution (.)", "content mismatch");
        remove_dir_recursive(subdir1);
        return;
    }
    TEST_PASS("path resolution (. current directory)");

    /* realpath 测试 */
    char resolved[PATH_MAX];
    if (realpath(dotdot_path, resolved) != NULL) {
        /* 确认 resolved 不包含 .. */
        if (strstr(resolved, "..") == NULL) {
            TEST_PASS("path resolution (realpath)");
        } else {
            TEST_FAIL("path resolution (realpath)",
                      "resolved path contains ..");
        }
    } else {
        TEST_FAIL("path resolution (realpath)", strerror(errno));
    }

    remove_dir_recursive(subdir1);
}

void run_functional_tests(const struct fstest_config *cfg) {
    printf("\n");
    printf("========================================\n");
    printf("  1. 功能正确性测试 (Functional Tests)\n");
    printf("========================================\n");

    test_create_delete_file(cfg);
    test_read_write_append_truncate(cfg);
    test_create_delete_directory(cfg);
    test_rename_move(cfg);
    test_permissions_timestamps(cfg);
    test_links(cfg);
    test_path_resolution(cfg);

    printf("--- 功能正确性测试完成 ---\n");
}
