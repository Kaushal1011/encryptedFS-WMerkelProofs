# encryptedFS-WMerkelProofs


# Compile Instructions

```bash
make
```


# Run Instructions

```bash
./encryptFS.out -f -d  ~/hello superblock1.bin key.txt 
```

or 
    
```bash
./encryptFS.out -f -d  ~/hello superblock1.bin 
```

when key is not provided, the program will generate a random key and store it in key.txt (dont forget to save it somewhere safe)

# Clean up 

```bash
make clean
```

# Unmount Instructions

```bash
fusermount -u ~/hello
```

# Test Instructions

```bash
cd ~/hello
touch cat.txt
echo "hello my name is cat. And I am very cute hehehe hello " > cat.txt
cat cat.txt
ls -l
```