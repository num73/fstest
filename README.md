文件系统测试代码的仓库

目录：

function   : 功能测试

performance： 性能测试

snippet: 文件系统相关的代码片段

# 文件系统测试

## 功能测试


## 性能测试 ##

### 性能指标 ### 


**Throughput** 吞吐量，单位时间内读/写的数据量。

**Latency** 延迟，一般定义为单个操作的时间。


### 影响性能指标的因素 ###

**`O_DIRECT`标记** 有些文件系统支持`O_DIRECT`标志，如果不加`O_DIRECT`标志，可能会通过缓存进行读写，比如ext4文件系统，如果不加`O_DIRECT`，对文件的读写会经过page cache，测出来的性能实际上是缓存的性能。


**numa节点** 如果机器上有多个numa节点，在不同numa节点的cpu和内存上执行的测试结果不同。

## 常用测试工具 ##

**iozone**

**FIO**

**ior**

**mdtest**

**filebench**

**YCSB**

**TPC-C**

## 常用i/o接口 ##

**read/write**

**pread/pwrite**

**mmap**

**io_uring**

# 文件系统 #

