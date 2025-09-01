获取系统时间，单位ns:

```c

// 头文件
#include <time.h>


#define NANOS_PER_SECOND (1000000000L)

// 计算是时间差，单位ns
inline int64_t calculate_time_diff_ns(struct timespec *start,
                                      struct timespec *end) {
    int64_t seconds = end->tv_sec - start->tv_sec;
    int64_t nanoseconds = end->tv_nsec - start->tv_nsec;
    return seconds * NANOS_PER_SECOND + nanoseconds;
}



struct timespec start;
struct timespec end;
clock_gettime(CLOCK_MONOTONIC, &start);

// do something...

clock_gettime(CLOCK_MONOTONIC, &end);



```

有时计算时间可能会遇到指令乱序执行的问题，需要在获取时间的地方前后都加上：

```c

#include <stdatomic.h>

atomic_thread_fence(memory_order_seq_cst);


```