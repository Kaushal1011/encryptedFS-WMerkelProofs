# encryptedFS-WMerkelProofs


# Compile Instructions
```bash
gcc encryptFs.c -o ./eFS.out `pkg-config fuse --cflags --libs`
```

OR

```bash
gcc -I./include main.c src/bitmap.c src/fs_operations.c src/inode.c src/volume.c -o myFS2 `pkg-config --cflags --libs fuse` -DFUSE_USE_VERSION=30
```

# Run Instructions

```bash
./eFS.out -f -d  ~/hello superblock.BIN
```

# Test Instructions

```bash
cd ~/hello
touch cat.txt
echo "hello my name is cat. And I am very cute hehehe hello " > cat.txt
cat cat.txt
ls -l
```