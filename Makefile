username := $(shell whoami)
mountpoint := /home/$(username)/hello
includepath := -I./include
srcprefix := ./src/
files := main.c $(srcprefix)fs_operations.c $(srcprefix)bitmap.c $(srcprefix)inode.c $(srcprefix)volume.c
cflags := -Wall $(includepath) -D_FILE_OFFSET_BITS=64 `pkg-config --cflags fuse` -DFUSE_USE_VERSION=30
ldflags := `pkg-config --libs fuse`
opflag := -o myFS

.PHONY: all run drun bgrun compile dcompile checkdir dmkfs mkfs_dcompile mkfs mkfs_compile cleanup

all: compile cleanup

run: compile cleanup
	$(opflag) -f $(mountpoint) /home/$(username)/file.txt

drun: dcompile cleanup
	$(opflag) -d -f -s $(mountpoint) /home/$(username)/file.txt

bgrun: compile
	$(opflag) $(mountpoint)

compile: checkdir
	gcc $(cflags) $(files) $(opflag) $(ldflags)

dcompile: checkdir
	gcc $(cflags) -g -DERR_FLAG $(files) $(opflag) $(ldflags)

checkdir:
	@[ -d "$(mountpoint)" ] || mkdir -p $(mountpoint)

dmkfs: mkfs_dcompile
	./main /home/$(username)/file.txt

mkfs_dcompile: checkdir
	gcc $(cflags) -g -DERR_FLAG $(files) -o main $(ldflags)

mkfs: mkfs_compile
	./main /home/$(username)/file.txt

mkfs_compile: checkdir
	gcc $(cflags) $(files) -o main $(ldflags)

# cleanup:
# 	-fusermount -u $(mountpoint) > /dev/null 2>&1