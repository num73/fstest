https://github.com/robinlee09201/ctFS

## 环境配置： ##





1. 把pmem配置成devdax

```sh
sudo ndctl disable-namespace namespace0.0
sudo ndctl destroy-namespace namespace0.0 --force
sudo ndctl create-namespace -m devdax 
```

2. 初始化ctfs


```sh
test/mkfs 1
```

3. 

```sh
numactl --cpunodebind=1 --membind=1 
```