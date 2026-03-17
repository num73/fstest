# fstest - 文件系统综合测试工具 Makefile

CC ?= gcc
CFLAGS = -Wall -Wextra -Wno-format-truncation -O2 -std=gnu11
LDFLAGS = -lpthread

# 源文件
SRC_DIR = src
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/common.c \
       $(SRC_DIR)/test_functional.c \
       $(SRC_DIR)/test_consistency.c \
       $(SRC_DIR)/test_exception.c \
       $(SRC_DIR)/test_concurrent.c \
       $(SRC_DIR)/test_stress.c \
       $(SRC_DIR)/test_performance.c

# 目标
TARGET = fstest

.PHONY: all clean help

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

help:
	@echo "Usage:"
	@echo "  make          - 编译 fstest"
	@echo "  make clean    - 清理编译产物"
	@echo "  make help     - 显示帮助"
	@echo ""
	@echo "运行示例:"
	@echo "  ./fstest -d /tmp/fstest_data              # 运行所有测试"
	@echo "  ./fstest -d /tmp/fstest_data -m 1         # 功能正确性测试"
	@echo "  ./fstest -d /mnt/nufs -m 6 -j 4 -s 4096  # 性能测试"
