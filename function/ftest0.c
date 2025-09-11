/*
    io_uring 示例程序

    编译时需要链接 liburing 库:
    gcc -o ftest0 ftest0.c -luring
*/

#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 4096

int main() {
    struct io_uring ring;
    io_uring_queue_init(8, &ring, 0);

    int fd = open("test.txt", O_RDWR | O_CREAT, 0644);
    char buf[BUF_SIZE] = "Hello io_uring!\n";

    // 写文件
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_write(sqe, fd, buf, strlen(buf), 0);
    io_uring_submit(&ring);

    struct io_uring_cqe *cqe;
    io_uring_wait_cqe(&ring, &cqe);
    io_uring_cqe_seen(&ring, cqe);

    // 读文件
    memset(buf, 0, BUF_SIZE);
    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, buf, BUF_SIZE, 0);
    io_uring_submit(&ring);

    io_uring_wait_cqe(&ring, &cqe);
    printf("Read: %s", buf);
    io_uring_cqe_seen(&ring, cqe);

    close(fd);
    io_uring_queue_exit(&ring);
    return 0;
}