#define FUSE_USE_VERSION 30

#include <libgen.h>
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include <linux/stat.h>
#include <math.h>

#include "bitmap.h"
#include "fs_operations.h"
#include "constants.h"
#include "inode.h"
#include "volume.h"


int allocate_inode_bmp(bitmap_t *bmp, const char *volume_id)
{
    for (int i = 0; i < INODES_PER_VOLUME; ++i)
    {
        if (is_bit_free(bmp->inode_bmp, i))
        {
            // Set the inode as used
            set_bit(bmp->inode_bmp, i);

            // Write the updated bitmap back to the file
            write_bitmap(volume_id, bmp);

            return i; // Return the index of the allocated inode
        }
    }
    return -1; // No free inode found
}

void add_inode_to_directory(int dir_inode_index, int file_inode_index)
{
}

// temporary implementation
int find_inode_index_by_path(const char *volume_id, const char *target_path)
{
    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);

    FILE *file = fopen(inode_filename, "rb");

    if (!file)
    {
        printf("inodes file can not be opened\n");
        // Inode file could not be opened; handle error appropriately.
        return -1;
    }

    inode temp_inode;
    int inode_index = 0;

    // Loop through all inodes in the volume
    while (fread(&temp_inode, sizeof(inode), 1, file) == 1)
    {
        // printf("temp_inode.path: %s\n", temp_inode.path);
        if (temp_inode.valid && strcmp(temp_inode.path, target_path) == 0)
        {
            fclose(file);
            return inode_index; // Found the inode for the given path
        }
        inode_index++;
    }

    fclose(file);
    return -1; // Target path not found
}




int main(int argc, char *argv[])
{
    superblock_t sb;

    // Check if a superblock path is provided as an argument
    if (argc < 3)
    {
        printf("Usage: %s <mountpoint> <superblock_path>\n", argv[0]);
        return 1;
    }

    // The superblock path is provided as the last argument for simplicity
    char *superblock_path = argv[argc - 1];

    // Modify the argument list to remove the superblock path
    argc--;

    // Attempt to load the superblock, or create a new one if it doesn't exist
    load_or_create_superblock(superblock_path, &sb);

    // Proceed with FUSE main loop
    return fuse_main(argc, argv, &fs_operations, NULL);
}