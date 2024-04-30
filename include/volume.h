#ifndef VOLUME_H
#define VOLUME_H

#include <sys/types.h>
#include "constants.h"
#include "merkle.h"

typedef enum volume_type
{
    LOCAL,
    AWS,
    GDRIVE,
    FTP
} volume_type;

typedef struct volume_info
{
    char inodes_path[MAX_PATH_LENGTH];
    char bitmap_path[MAX_PATH_LENGTH];
    char volume_path[MAX_PATH_LENGTH];
    char merkle_path[MAX_PATH_LENGTH];
    int inodes_count;
    int blocks_count;
    MerkleTree *merkle_tree; // Pointer to the Merkle tree of this volume
} volume_info_t;

typedef struct superblock
{
    int volume_count;
    int block_size;
    int inode_size;
    volume_type vtype;
    volume_info_t volumes[NUMVOLUMES]; // Assume a maximum of 10 volumes
} superblock_t;

extern superblock_t sb;

extern char superblock_path[MAX_PATH_LENGTH];

// Function prototypes for volume operations
void init_volume(volume_info_t *volume, const char *path, volume_type type, int i);
void load_or_create_superblock(const char *path, superblock_t *sb);
void init_superblock_local(superblock_t *sb);
void create_volume_files_local(int i, superblock_t *sb);
void read_volume_block(int block_index, void *buf);
void read_volume_block_no_check(int block_index, void *buf);
void write_volume_block(int block_index, const void *buf, size_t buf_size);

#endif // VOLUME_H
