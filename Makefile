username := $(shell whoami)
mountpoint := /home/$(username)/hello
includepath := -I./include
srcprefix := ./src/
files := main.c $(srcprefix)fs_operations.c $(srcprefix)bitmap.c $(srcprefix)inode.c $(srcprefix)volume.c $(srcprefix)merkle.c $(srcprefix)crypto.c 
cflags := -Wall $(includepath) -D_FILE_OFFSET_BITS=64 `pkg-config --cflags fuse openssl libsodium` -DFUSE_USE_VERSION=30
ldflags := `pkg-config --libs fuse openssl libsodium`
opflag := -o encryptFS.out

.PHONY: all run drun bgrun compile dcompile checkdir dmkfs mkfs_dcompile mkfs mkfs_compile cleanup

all: compile 

clean:
	-rm -f encryptFS.out 
	-rm -rf *.bin
keygen:
	./encryptFS.out keygen ./key.txt
run: 
	./encryptFS.out -f $(mountpoint) ./superblock.bin ./key.txt
drun: 
	./encryptFS.out -d -f -s $(mountpoint) ./superblock.bin ./key.txt
bgrun: compile
	./encryptFS.out $(mountpoint)
compile: checkdir
	gcc $(cflags) $(files) $(opflag) $(ldflags)
dcompile: checkdir
	gcc $(cflags) -g -DERR_FLAG $(files) $(opflag) $(ldflags)
checkdir:
	@[ -d "$(mountpoint)" ] || mkdir -p $(mountpoint)
unmount:
	fusermount -u $(mountpoint)
