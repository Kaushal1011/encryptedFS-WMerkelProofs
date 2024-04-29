#include "volume.h"
#include "bitmap.h"
#include "inode.h"
#include "merkle.h"
#include "constants.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

superblock_t sb;

// Initialize a volume with default paths and settings
void init_volume(volume_info_t *volume, const char *path, volume_type type, int volume_id)
{
    snprintf(volume->inodes_path, MAX_PATH_LENGTH, "%s/inodes_%d.bin", path, volume_id);
    snprintf(volume->bitmap_path, MAX_PATH_LENGTH, "%s/bmp_%d.bin", path, volume_id);
    snprintf(volume->volume_path, MAX_PATH_LENGTH, "%s/volume_%d.bin", path, volume_id);
    snprintf(volume->merkle_path, MAX_PATH_LENGTH, "%s/merkle_%d.bin", path, volume_id);
    volume->inodes_count = INODES_PER_VOLUME;
    volume->blocks_count = DATA_BLOCKS_PER_VOLUME;
    volume->merkle_tree = NULL;
    // volume->merkle_tree = initialize_merkle_tree_for_volume(volume->volume_path);
    // save_merkle_tree(volume->merkle_tree, volume->merkle_path);
}

// Load or create a superblock for the filesystem
void load_or_create_superblock(const char *path, superblock_t *sb)
{
    FILE *file = fopen(path, "rb+");
    if (!file)
    {
        printf("Superblock file not found, creating a new one.\n");
        file = fopen(path, "wb+");
        for (int i = 0; i < 10; i++)
        {                                                 // Initialize each volume info
            init_volume(&sb->volumes[i], "./", LOCAL, i); // Default initialization
        }
        sb->volume_count = 1; // Start with one volume
        sb->block_size = BLOCK_SIZE;
        sb->inode_size = INODE_SIZE;

        printf("here\n");

        // create the first volume

        if (strstr(path, "https") == NULL)
        {
            sb->vtype = LOCAL;
            printf("Local volume init\n");
            create_volume_files_local(0, sb);
            // Create the root directory inode
            inode root_inode;
            init_inode(&root_inode, "/", S_IFDIR | 0777); // Root directory
            write_inode("0", 0, &root_inode);             // Assuming the root inode is in volume 0
            // Create the root directory bitmap
            bitmap_t root_bmp;
            memset(root_bmp.inode_bmp, 0, sizeof(root_bmp.inode_bmp));
            memset(root_bmp.datablock_bmp, 0, sizeof(root_bmp.datablock_bmp));
            set_bit(root_bmp.inode_bmp, 0); // Mark the root inode as used
            write_bitmap("0", &root_bmp);   // Assuming the root bitmap is in volume 0
        }
        else
        {
            // remote volume init is missing
            //  add support for remote volume here
        }

        fwrite(sb, sizeof(superblock_t), 1, file);
    }
    else
    {
        fread(sb, sizeof(superblock_t), 1, file);
        // Load merkle trees for each volume , the stored pointer will be invalid
        int num_vol = sb->volume_count;
        for (int i = 0; i < num_vol; i++)
        {
            printf("Loading merkle tree for volume %d\n", i);
            // load merkle tree for each volume
            sb->volumes[i].merkle_tree = load_merkle_tree_from_file(sb->volumes[i].merkle_path);
        }
    }
    fclose(file);
}

void create_volume_files_local(int i, superblock_t *sb)
{
    printf("Creating volume files for volume %d\n", i);

    FILE *inodes_file = fopen(sb->volumes[i].inodes_path, "w");
    fclose(inodes_file);
    FILE *bitmap_file = fopen(sb->volumes[i].bitmap_path, "w");
    fclose(bitmap_file);
    FILE *volume_file = fopen(sb->volumes[i].volume_path, "w");
    fclose(volume_file);

    printf("Volume files created for volume %d\n", i);

    FILE *merkle_file = fopen(sb->volumes[i].merkle_path, "wb+");
    if (!merkle_file)
    {
        printf("Merkle file not found, creating a new one.\n");
        MerkleTree *merkle_tree = initialize_merkle_tree_for_volume(sb->volumes[i].volume_path);
        save_merkle_tree_to_file(merkle_tree, sb->volumes[i].merkle_path);
        sb->volumes[i].merkle_tree = merkle_tree;
    }
    else
    {
        MerkleTree *merkle_tree = initialize_merkle_tree_for_volume(sb->volumes[i].volume_path);
        save_merkle_tree_to_file(merkle_tree, sb->volumes[i].merkle_path);
        sb->volumes[i].merkle_tree = merkle_tree;
    }
    fclose(merkle_file);
}

void read_volume_block_no_check(char *volume_id, int block_index, void *buf)
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

    printf("Reading block %d from volume %s\n", block_index, volume_id);
}

void read_volume_block(char *volume_id, int block_index, void *buf)
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

    printf("Reading block %d from volume %s\n", block_index, volume_id);

    if (!verify_block_integrity(volume_id, block_index))
    {
        printf("Integrity check failed for block %d in volume %s\n", block_index, volume_id);
        // Handle the integrity check failure as needed
    }
}

void write_volume_block(char *volume_id, int block_index, const void *buf)
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

    // read the whole block from the volume to update the merkle tree
    char block_data[BLOCK_SIZE];

    read_volume_block_no_check(volume_id, block_index, block_data);

    //  TODO: Read before updating the block

    update_merkle_node_for_block(volume_id, block_index, block_data);
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
    sb->volumes[0].inodes_count = INODES_PER_VOLUME;
    sb->volumes[0].blocks_count = DATA_BLOCKS_PER_VOLUME;
}
