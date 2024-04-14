#include "volume.h"
#include <stdio.h>
#include <string.h>

// Initialize a volume with default paths and settings
void init_volume(volume_info_t *volume, const char *path, volume_type type) {
    snprintf(volume->inodes_path, MAX_PATH_LENGTH, "%s/inodes.bin", path);
    snprintf(volume->bitmap_path, MAX_PATH_LENGTH, "%s/bitmap.bin", path);
    snprintf(volume->volume_path, MAX_PATH_LENGTH, "%s/volume.bin", path);
    volume->inodes_count = INODES_PER_VOLUME;
    volume->blocks_count = DATA_BLOCKS_PER_VOLUME;
    volume->type = type;
}

// Load or create a superblock for the filesystem
void load_or_create_superblock(const char *path, superblock_t *sb) {
    FILE *file = fopen(path, "rb+");
    if (!file) {
        printf("Superblock file not found, creating a new one.\n");
        file = fopen(path, "wb+");
        for (int i = 0; i < 10; i++) {  // Initialize each volume info
            init_volume(&sb->volumes[i], "/default/path", LOCAL);  // Default initialization
        }
        sb->volume_count = 1;  // Start with one volume
        sb->block_size = BLOCK_SIZE;
        sb->inode_size = INODE_SIZE;
        fwrite(sb, sizeof(superblock_t), 1, file);
    } else {
        fread(sb, sizeof(superblock_t), 1, file);
    }
    fclose(file);
}

void read_volume_block(const char *volume_id, int block_index, void *buf)
{
    char volume_filename[256];
    sprintf(volume_filename, "volume_%s.bin", volume_id);
    FILE *file = fopen(volume_filename, "rb");
    if (file)
    {
        fseek(file, block_index * BLOCK_SIZE, SEEK_SET);
        fread(buf, BLOCK_SIZE, 1, file);
        fclose(file);
    }
    else
    {
        // Handle error: volume file not found
    }
}

void write_volume_block(const char *volume_id, int block_index, const void *buf)
{
    char volume_filename[256];
    sprintf(volume_filename, "volume_%s.bin", volume_id);
    FILE *file = fopen(volume_filename, "r+b"); // Open for reading and writing; binary mode
    if (file)
    {
        fseek(file, block_index * BLOCK_SIZE, SEEK_SET);
        fwrite(buf, BLOCK_SIZE, 1, file);
        fclose(file);
    }
    else
    {
        // Handle error: volume file not found or unable to write
    }
}

// Function to initialize a new superblock
void init_superblock_local(superblock_t *sb)
{
    sb->volume_count = 1; // Start with one volume
    sb->block_size = BLOCK_SIZE;
    sb->inode_size = INODE_SIZE;
    // Initialize first volume (Example paths, modify as needed)
    strcpy(sb->volumes[0].inodes_path, "./inodes_0.bin");
    strcpy(sb->volumes[0].bitmap_path, "./bmp_0.bin");
    strcpy(sb->volumes[0].volume_path, "./volume_0.bin");
    sb->volumes[0].type = LOCAL;
    sb->volumes[0].inodes_count = INODES_PER_VOLUME;
    sb->volumes[0].blocks_count = DATA_BLOCKS_PER_VOLUME;
}
