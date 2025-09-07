https://github.com/utsaslab/SplitFS

```sh
numactl --physcpubind=0-15 --membind=0 env LD_PRELOAD=./splitfs/libnvp.so ./test4
```