```sh
sudo mkfs.ext4 -b 4096 -E stride=512 -F /dev/pmem0
sudo mount -o dax /dev/pmem0 /mnt/pmemsh
```