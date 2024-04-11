# encryptedFS-WMerkelProofs


# Compile Instructions
```bash
gcc encryptFs.c -o ./eFS.out `pkg-config fuse --cflags --libs`
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