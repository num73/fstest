文件系统测试代码的仓库

# fstest - 文件系统综合测试工具

单个可执行程序，通过命令行选项选择不同的测试功能。

## 编译

```bash
make
```

## 使用方法

```bash
./fstest -d <测试目录> [-m <模式>] [-j <线程数>] [-s <IO大小>] [-f <文件大小MB>] [-i <迭代次数>] [-v]
```

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-d <dir>` | 测试文件存放目录（必须） | - |
| `-m <mode>` | 测试模式（使用英文单词） | `all` |
| `-j <n>` | 并发线程数 | 1 |
| `-s <bytes>` | IO 大小（字节） | 4096 |
| `-f <MB>` | 测试文件大小（MB） | 256 |
| `-i <n>` | 迭代次数 | 5 |
| `-v` | 详细输出 | - |
| `-h` | 显示帮助 | - |

说明：当前推荐使用英文模式名；为兼容旧脚本，程序仍接受历史数字别名 `0-6`。

### 测试模式

| 模式 | 说明 |
|------|------|
| `all` | 运行所有测试 |
| `functional` | 功能正确性测试 |
| `consistency` | 数据一致性测试 |
| `exception` | 异常场景测试 |
| `concurrent` | 并发测试 |
| `stress` | 压力和稳定性测试 |
| `performance` | 性能测试 |

### 示例

```bash
# 运行所有测试
./fstest -d /tmp/fstest_data

# 仅运行功能正确性测试
./fstest -d /tmp/fstest_data -m functional

# 性能测试，4线程，4KB IO
./fstest -d /mnt/nufs -m performance -j 4 -s 4096

# 并发测试，8线程
./fstest -d /tmp/fstest_data -m concurrent -j 8

# 压力测试，小文件大小
./fstest -d /tmp/fstest_data -m stress -f 16 -i 2
```

## 测试类别

### 1. 功能正确性测试 (`-m functional`)

验证基本文件系统操作是否正确：

- 创建 / 删除文件
- 读 / 写 / 追加 / 截断
- 创建 / 删除目录
- 重命名、移动
- 权限、时间戳、属性检查
- 硬链接 / 软链接
- 路径解析（相对路径、绝对路径、`.`、`..`）

### 2. 数据一致性测试 (`-m consistency`)

确保写入的数据读回后是正确的：

- 写后读校验
- 校验和 / CRC32 比对
- 随机读写一致性检查
- 大文件、小文件、空文件测试
- 稀疏文件测试
- 反复覆盖写后的结果验证

### 3. 异常场景测试 (`-m exception`)

测试文件系统在异常条件下的行为：

- 权限不足 (EACCES)
- 文件锁定 / 被占用
- 不存在的文件操作 (ENOENT)
- 非空目录删除 (ENOTEMPTY)
- 中断写操作后的文件可访问性
- 边界条件（零长度写、超长文件名）

### 4. 并发测试 (`-m concurrent`)

验证多线程并发操作的可靠性：

- 并发读写同一文件
- 并发创建 / 删除文件
- 并发目录操作
- 文件锁测试（带锁计数器）
- 竞争条件检测

### 5. 压力和稳定性测试 (`-m stress`)

测试高负载下的表现：

- 海量小文件（1000个文件创建/删除）
- 超大文件读写
- 深层目录结构（50层）
- 高频创建 / 删除 / 重命名
- 循环读写一致性验证

### 6. 性能测试 (`-m performance`)

衡量文件系统性能表现：

- 基于常规 `read/write` 的顺序读写吞吐量
- 基于常规 `read/write` 的随机读写吞吐量
- 基于 `O_DIRECT` 的顺序/随机读写吞吐量
- 基于 `mmap` 的顺序/随机读写吞吐量
- 不同 IO 大小下的表现
- 读写延迟统计（平均、最小、最大）
- 元数据操作性能（create / stat / rename / unlink）

实现上，这一组测试会分别从三种访问路径观察文件系统性能：普通 `read/write`、尽量绕过页缓存的 `O_DIRECT`，以及基于内存映射的 `mmap`。

- `O_DIRECT` 路径包含顺序读、顺序写、随机读、随机写四项；如果当前文件系统、挂载方式或内核不支持，则会输出 `SKIP`，不会让整组性能测试失败。
- 由于 `O_DIRECT` 对齐要求较严格，测试时会把 `IO size` 向上对齐到 `4096` 字节后再执行。
- `mmap` 路径同样覆盖顺序读、顺序写、随机读、随机写；其中写测试使用共享映射，并在每轮迭代后执行 `msync(MS_SYNC)`，因此结果更接近“映射写入并同步落盘”的开销。

输出中会看到类似下面几类标签：

- `Sequential Read` / `Sequential Write` / `Random Read` / `Random Write`
- `Sequential Read (O_DIRECT)` / `Sequential Write (O_DIRECT)` / `Random Read (O_DIRECT)` / `Random Write (O_DIRECT)`
- `Sequential Read (mmap)` / `Sequential Write (mmap)` / `Random Read (mmap)` / `Random Write (mmap)`

## 目录结构

```
src/                    # 统一测试工具源码
  main.c                # 入口、选项解析、测试调度
  common.h / common.c   # 公共定义和工具函数
  test_functional.c     # 功能正确性测试
  test_consistency.c    # 数据一致性测试
  test_exception.c      # 异常场景测试
  test_concurrent.c     # 并发测试
  test_stress.c         # 压力和稳定性测试
  test_performance.c    # 性能测试
Makefile                # 编译构建

```

