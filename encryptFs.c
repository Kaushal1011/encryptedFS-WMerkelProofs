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

#define BLOCK_SIZE 4096
#define INODE_SIZE 512
#define INODES_PER_VOLUME 32768
#define DATA_BLOCKS_PER_VOLUME 32768
#define MAX_CHILDREN 128
#define MAX_PATH_LENGTH 256
#define MAX_NAME_LENGTH 256
#define MAX_TYPE_LENGTH 20
#define MAX_DATABLOCKS 16
#define ENCRYPTION_KEY_SIZE 32 // AES-256
#define AES_GCM_NONCE_SIZE 12
#define AES_GCM_TAG_SIZE 16

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef enum volume_type
{
    LOCAL,
    FTP,
    GDRIVE,
} volume_type;

typedef struct volume_info
{
    char inodes_path[256]; // path to the volume file that stores inodes
    char bitmap_path[256]; // Path to the volume file that stores bitmaps
    char volume_path[256]; // Path to the volume file that stores data blocks
    int inodes_count;      // Total inodes in this volume
    int blocks_count;      // Total data blocks in this volume
    volume_type type;
} volume_info_t;

typedef struct superblock
{
    int volume_count;          // Number of volumes in the filesystem
    int block_size;            // Block size (should be consistent across volumes)
    int inode_size;            // Inode size (should be consistent)
    volume_info_t volumes[10]; // Information for up to 10 volumes (expandable)
} superblock_t;

typedef struct bitmap
{
    char inode_bmp[4096];
    char datablock_bmp[4096];
} bitmap_t;

typedef struct inode
{
    int valid;                  // Indicates if the inode is in use
    int inode_number;           // Unique inode number
    char path[MAX_PATH_LENGTH]; // Full path
    char name[MAX_NAME_LENGTH]; // Name of the file/directory
    mode_t permissions;         // Permissions
    bool is_directory;          // Indicates if the inode represents a directory
    uid_t user_id;              // Owner's user ID
    gid_t group_id;             // Owner's group ID
    time_t a_time;              // Last access time
    time_t m_time;              // Last modification time
    time_t c_time;              // Last status change time
    time_t b_time;              // Creation time
    off_t size;                 // Size in bytes

    unsigned char encryption_key[ENCRYPTION_KEY_SIZE]; // AES encryption key
    unsigned char nonce[AES_GCM_NONCE_SIZE];           // Nonce for AES-GCM
    unsigned char auth_tag[AES_GCM_TAG_SIZE];          // Authentication tag for AES-GCM

    int datablocks[MAX_DATABLOCKS]; // Data block indices
    int num_datablocks;             // Number of data blocks used

    int parent_inode;           // Inode number of the parent directory
    int children[MAX_CHILDREN]; // Inode numbers of the children
    int num_children;           // Number of children

    char type[MAX_TYPE_LENGTH]; // File extension/type
    int num_links;              // Number of links
} inode;

void read_bitmap(const char *volume_id, bitmap_t *bmp)
{
    char bmp_filename[256];
    sprintf(bmp_filename, "bmp_%s.bin", volume_id);
    FILE *file = fopen(bmp_filename, "rb");
    if (file)
    {
        fread(bmp, sizeof(bitmap_t), 1, file);
        fclose(file);
    }
    else
    {
        // Handle error: bitmap file not found
    }
}

void write_bitmap(const char *volume_id, const bitmap_t *bmp)
{
    char bmp_filename[256];
    sprintf(bmp_filename, "bmp_%s.bin", volume_id);
    FILE *file = fopen(bmp_filename, "wb"); // Open for writing in binary mode
    if (file)
    {
        fwrite(bmp, sizeof(bitmap_t), 1, file);
        fclose(file);
    }
    else
    {
        // Handle error: Unable to open bitmap file for writing
    }
}

void read_inode(const char *volume_id, int inode_index, inode *inode_buf)
{
    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);
    FILE *file = fopen(inode_filename, "rb");
    if (file)
    {
        fseek(file, inode_index * sizeof(inode), SEEK_SET);
        fread(inode_buf, sizeof(inode), 1, file);
        fclose(file);
    }
    else
    {
        // Handle error: inode file not found
    }
}

void write_inode(const char *volume_id, int inode_index, const inode *inode_buf)
{
    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);
    FILE *file = fopen(inode_filename, "r+b"); // Open for reading and writing in binary mode
    if (file)
    {
        fseek(file, inode_index * sizeof(inode), SEEK_SET);
        fwrite(inode_buf, sizeof(inode), 1, file);
        fclose(file);
    }
    else
    {
        // Handle error: Unable to open inode file for writing
    }
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

void set_bit(unsigned char *bitmap, int index)
{
    int byte_index = index / 8;
    int bit_index = index % 8;
    bitmap[byte_index] |= (1 << bit_index);
}

void clear_bit(unsigned char *bitmap, int index)
{
    int byte_index = index / 8;
    int bit_index = index % 8;
    bitmap[byte_index] &= ~(1 << bit_index);
}

// Utility function to check if the bit at `index` in `bitmap` is 0 (free)
bool is_bit_free(unsigned char *bitmap, int index)
{
    int byte_index = index / 8;
    int bit_index = index % 8;
    return !(bitmap[byte_index] & (1 << bit_index));
}

int allocate_inode_bmp(bitmap_t *bmp, const char *volume_id)
{
    printf("Allocating inode for volume %s\n", volume_id);

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

void init_inode(inode *node, const char *path, mode_t mode)
{
    // Initialize the inode fields
    node->valid = 1;
    strncpy(node->path, path, MAX_PATH_LENGTH - 1);
    node->path[MAX_PATH_LENGTH - 1] = '\0'; // Ensure null termination

    // Extract file name from path
    char *pathCopy = strdup(path);       // Duplicate path since basename might modify it
    char *fileName = basename(pathCopy); // Extracts the base name of the file
    strncpy(node->name, fileName, MAX_NAME_LENGTH - 1);
    node->name[MAX_NAME_LENGTH - 1] = '\0'; // Ensure null termination
    free(pathCopy);                         // Clean up the duplicated path

    // Set permissions
    node->permissions = mode;
    node->is_directory = S_ISDIR(mode);

    // Initialize ownership to the current process's owner
    node->user_id = getuid();
    node->group_id = getgid();

    // Initialize timestamps
    time_t now = time(NULL);
    node->a_time = now; // Last access time
    node->m_time = now; // Last modification time
    node->c_time = now; // Last status change time
    node->b_time = now; // Creation time

    // Initialize size and data blocks
    node->size = 0;           // Assuming the new inode represents a file that is initially empty
    node->num_datablocks = 0; // No data blocks allocated yet
    for (int i = 0; i < MAX_DATABLOCKS; ++i)
    {
        node->datablocks[i] = -1; // Initialize all data block indices to -1 indicating they are not used
    }

    // Assuming it's a file for now, so no children
    node->num_children = 0;
    node->parent_inode = -1; // If creating a root inode or parent is not known at this stage

    // Initialize encryption-related fields if necessary

    // Initialize the file type and link count
    node->type[0] = '\0'; // Assuming the type needs to be determined elsewhere or is not applicable
    node->num_links = 1;  // A newly created file typically has one link
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

// Function to initialize a new superblock for remote volumes
// modify later when doing cloud implementation
void init_superblock_remote(superblock_t *sb)
{
    sb->volume_count = 1; // Start with one volume
    sb->block_size = BLOCK_SIZE;
    sb->inode_size = INODE_SIZE;
    // Initialize first volume (Example paths, modify as needed)
    strcpy(sb->volumes[0].inodes_path, "https://example.com/inodes_0.bin");
    strcpy(sb->volumes[0].bitmap_path, "https://example.com/bmp_0.bin");
    strcpy(sb->volumes[0].volume_path, "https://example.com/volume_0.bin");
    sb->volumes[0].type = FTP; // Example: FTP
    sb->volumes[0].inodes_count = INODES_PER_VOLUME;
    sb->volumes[0].blocks_count = DATA_BLOCKS_PER_VOLUME;
}

// Function to load or create superblock
void load_or_create_superblock(const char *path, superblock_t *sb)
{
    FILE *file = fopen(path, "r");
    if (file)
    {
        // Load superblock from file
        fread(sb, sizeof(superblock_t), 1, file);
        fclose(file);
    }
    else
    {
        // No superblock found, initialize a new one
        // check if superblock path doesnt contain https
        if (strstr(path, "https") == NULL)
        {
            init_superblock_local(sb);
        }
        else
        {
            // Initialize superblock for remote volumes
            init_superblock_remote(sb);
        }

        // Save the new superblock to disk
        file = fopen(path, "w");
        fwrite(sb, sizeof(superblock_t), 1, file);
        fclose(file);

        // Create the volume files (inodes, bitmap, volume) for each volume
        for (int i = 0; i < sb->volume_count; ++i)
        {
            FILE *inodes_file = fopen(sb->volumes[i].inodes_path, "w");
            fclose(inodes_file);
            FILE *bitmap_file = fopen(sb->volumes[i].bitmap_path, "w");
            fclose(bitmap_file);
            FILE *volume_file = fopen(sb->volumes[i].volume_path, "w");
            fclose(volume_file);
        }

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
}

int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{

    printf("in create\n");

    (void)fi; // The fuse_file_info is not used in this simple example

    // Assuming a global or externally accessible superblock `sb` and volume ID `volume_id`
    extern superblock_t sb;
    // right now kept zero but read it from the superblock
    const char *volume_id = "0";

    // Load the current bitmap to find a free inode
    bitmap_t bmp;
    read_bitmap(volume_id, &bmp);

    // Allocate a new inode for the file
    int inode_index = allocate_inode_bmp(&bmp, volume_id);
    if (inode_index == -1)
    {
        return -ENOSPC; // No space left for new inode
    }

    printf("inode_index: %d\n", inode_index);

    // Initialize the new inode
    inode new_inode;
    init_inode(&new_inode, path, mode);

    // Write the new inode to the inode file
    write_inode(volume_id, inode_index, &new_inode);

    inode root_inode;
    read_inode(volume_id, 0, &root_inode); // root inode is at index 0

    if (root_inode.num_children < MAX_CHILDREN)
    {
        root_inode.children[root_inode.num_children++] = inode_index;
        write_inode(volume_id, 0, &root_inode); // Update root inode
    }
    else
    {
        return -ENOSPC; // No space left
    }

    return 0; // Success
}

// fs_read
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("read\n");

    (void)fi; // Unused in this simplified example, but can be used for caching inode index etc.

    inode file_inode;
    const char *volume_id = "0";                                 // Assuming single volume setup for simplicity
    int inode_index = find_inode_index_by_path(volume_id, path); // Implement this function to find inode by path

    if (inode_index < 0)
    {
        return -ENOENT; // No such file
    }

    // Load the inode information
    read_inode(volume_id, inode_index, &file_inode);

    if (file_inode.is_directory)
    {
        return -EISDIR; // Is a directory, not a file
    }

    size_t bytes_read = 0;
    size_t remaining = size;
    off_t pos = offset;

    // Loop over the inode's data blocks to read data until size is reached or end of file
    while (remaining > 0 && pos < file_inode.size)
    {
        int block_index = pos / BLOCK_SIZE;
        off_t block_offset = pos % BLOCK_SIZE;
        size_t bytes_to_read = BLOCK_SIZE - block_offset < remaining ? BLOCK_SIZE - block_offset : remaining;

        if (block_index >= file_inode.num_datablocks)
        {
            break; // Trying to read beyond the last data block
        }

        char block_data[BLOCK_SIZE];
        read_volume_block(volume_id, file_inode.datablocks[block_index], block_data);

        memcpy(buf + bytes_read, block_data + block_offset, bytes_to_read);

        bytes_read += bytes_to_read;
        remaining -= bytes_to_read;
        pos += bytes_to_read;
    }

    return bytes_read;
}

// Utility function to find the first free block in the bitmap and allocate it.
int allocate_data_block(bitmap_t *bmp, const char *volume_id)
{
    printf("Allocating data block for %s\n", volume_id);
    for (int i = 0; i < DATA_BLOCKS_PER_VOLUME; ++i)
    {
        if (is_bit_free(bmp->datablock_bmp, i))
        {
            set_bit(bmp->datablock_bmp, i); // Mark the block as used
            write_bitmap(volume_id, bmp);   // Persist the updated bitmap
            return i;                       // Return the index of the newly allocated block
        }
    }
    return -1; // No free block found
}

int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("write\n");

    (void)fi; // Unused in this example

    const char *volume_id = "0"; // Assuming single volume setup
    bitmap_t bmp;
    read_bitmap(volume_id, &bmp);

    inode file_inode;
    int inode_index = find_inode_index_by_path(volume_id, path);
    if (inode_index < 0)
        return -ENOENT;

    read_inode(volume_id, inode_index, &file_inode);
    if (file_inode.is_directory)
        return -EISDIR;

    size_t bytes_written = 0;
    off_t pos = offset;

    while (bytes_written < size)
    {
        int block_index = pos / BLOCK_SIZE;
        off_t block_offset = pos % BLOCK_SIZE;
        size_t bytes_to_write = MIN(BLOCK_SIZE - block_offset, size - bytes_written);

        if (block_index >= file_inode.num_datablocks)
        {
            // Allocate a new block
            int new_block_index = allocate_data_block(&bmp, volume_id);
            if (new_block_index == -1)
                return -ENOSPC; // No space left

            file_inode.datablocks[block_index] = new_block_index;
            file_inode.num_datablocks += 1;
        }

        char block_data[BLOCK_SIZE] = {0};
        // Read existing block data if not writing a full block
        if (bytes_to_write < BLOCK_SIZE)
        {
            read_volume_block(volume_id, file_inode.datablocks[block_index], block_data);
        }

        // Copy data to block
        memcpy(block_data + block_offset, buf + bytes_written, bytes_to_write);
        write_volume_block(volume_id, file_inode.datablocks[block_index], block_data);

        bytes_written += bytes_to_write;
        pos += bytes_to_write;
    }

    // Update file size
    if (offset + size > file_inode.size)
    {
        file_inode.size = offset + size;
    }
    write_inode(volume_id, inode_index, &file_inode);

    return bytes_written;
}

static int fs_truncate(const char *path, off_t newsize)
{
    printf("truncate\n");

    const char *volume_id = "0"; // Assuming single volume setup for simplicity
    int inode_index = find_inode_index_by_path(volume_id, path);
    if (inode_index < 0)
    {
        return -ENOENT; // File not found
    }

    inode file_inode;
    read_inode(volume_id, inode_index, &file_inode);

    if (file_inode.is_directory)
    {
        return -EISDIR; // Cannot truncate a directory
    }

    bitmap_t bmp;
    read_bitmap(volume_id, &bmp);

    // Handling shrinking of the file
    if (newsize < file_inode.size)
    {
        // Calculate the number of blocks needed after truncation
        int new_blocks_needed = (newsize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        // Free blocks beyond the new size
        for (int i = new_blocks_needed; i < file_inode.num_datablocks; i++)
        {
            clear_bit(bmp.datablock_bmp, file_inode.datablocks[i]);
            file_inode.datablocks[i] = -1; // Mark the block as free
        }
        file_inode.num_datablocks = new_blocks_needed;
    }
    else if (newsize > file_inode.size)
    { // Handling expanding of the file
        int current_blocks = file_inode.num_datablocks;
        int required_blocks = (newsize + BLOCK_SIZE - 1) / BLOCK_SIZE;

        for (int i = current_blocks; i < required_blocks; i++)
        {
            int new_block_index = allocate_data_block(&bmp, volume_id);
            if (new_block_index == -1)
                return -ENOSPC; // No space left for new blocks

            file_inode.datablocks[i] = new_block_index;
            file_inode.num_datablocks += 1;

            // Initialize the new block to zero
            char zero_block[BLOCK_SIZE] = {0};
            write_volume_block(volume_id, new_block_index, zero_block);
        }
    }

    // Update the inode size and write back
    file_inode.size = newsize;
    write_inode(volume_id, inode_index, &file_inode);
    write_bitmap(volume_id, &bmp); // Make sure to write back the bitmap as well

    return 0; // Success
}

int fs_getattr(const char *path, struct stat *stbuf)
{
    printf("getattr\n");

    printf("path: %s\n", path);

    memset(stbuf, 0, sizeof(struct stat)); // Clear the stat buffer

    const char *volume_id = "0";
    int inode_index = find_inode_index_by_path(volume_id, path);
    if (inode_index == -1)
        return -ENOENT;

    inode node;
    read_inode(volume_id, inode_index, &node);
    if (!node.valid)
        return -ENOENT;

    stbuf->st_ino = inode_index; // Important for some operations to avoid confusion
    stbuf->st_mode = node.permissions | (node.is_directory ? S_IFDIR : S_IFREG);
    stbuf->st_nlink = node.num_links;
    stbuf->st_uid = node.user_id;
    stbuf->st_gid = node.group_id;
    stbuf->st_size = node.size;
    stbuf->st_atime = node.a_time;
    stbuf->st_mtime = node.m_time;
    stbuf->st_ctime = node.c_time;
    stbuf->st_blocks = (node.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    stbuf->st_blksize = BLOCK_SIZE;

    return 0;
}

int fs_open(const char *path, struct fuse_file_info *fi)
{
    printf("open\n");

    const char *volume_id = "0"; // Assuming single volume setup
    int inode_index = find_inode_index_by_path(volume_id, path);
    if (inode_index == -1)
        return -ENOENT; // No such file

    inode file_inode;
    read_inode(volume_id, inode_index, &file_inode);

    // Check if directory (directories cannot be opened)
    if (file_inode.is_directory)
        return -EISDIR;

    //  not handling file open modes and permissions.
    // real application,  would check `fi->flags` here and compare them with
    // the file's permissions to decide whether to allow the open operation.

    return 0;
}

int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("readdir\n");

    (void)offset; // Not used in this function
    (void)fi;     // Not used in this function

    const char *volume_id = "0"; // Assuming single volume setup
    // int dir_inode_index = find_inode_index_by_path(volume_id, path)
    int dir_inode_index = 0; // Assuming root directory for now

    if (dir_inode_index < 0)
    {
        return -ENOENT; // Directory not found
    }

    inode dir_inode;
    read_inode(volume_id, dir_inode_index, &dir_inode);

    if (!dir_inode.valid)
    {
        return -ENOENT; // Directory not valid
    }

    if (!dir_inode.is_directory)
    {
        return -ENOTDIR; // Not a directory
    }

    // Add the current (".") and parent ("..") directory entries
    if (filler(buf, ".", NULL, 0) != 0 || filler(buf, "..", NULL, 0) != 0)
    {
        return -ENOMEM; // Buffer full
    }

    // List child inodes
    for (int i = 0; i < dir_inode.num_children; i++)
    {
        if (dir_inode.children[i] == -1)
        {
            continue; // Skip uninitialized entries
        }

        inode child_inode;
        read_inode(volume_id, dir_inode.children[i], &child_inode);

        if (child_inode.valid)
        {
            printf("child_inode.name: %s\n", child_inode.name);
            if (filler(buf, child_inode.name, NULL, 0) != 0)
            {
                return -ENOMEM; // Buffer full
            }
        }
    }

    return 0; // Success
}

int fs_rename(const char *from, const char *to)
{
    printf("rename\n");

    const char *volume_id = "0"; // Assuming single volume setup

    // Find inode index for the source path
    int from_inode_index = find_inode_index_by_path(volume_id, from);
    if (from_inode_index < 0)
        return -ENOENT; // Source not found

    inode from_inode;
    read_inode(volume_id, from_inode_index, &from_inode);

    // Check if the target exists
    int to_inode_index = find_inode_index_by_path(volume_id, to);
    if (to_inode_index >= 0)
    {
        // For simplicity, let's return an error if the target exists
        return -EEXIST;
    }

    // Update the inode's path and name
    strncpy(from_inode.path, to, MAX_PATH_LENGTH - 1);
    char *baseName = basename(strdup(to)); // Duplicate since basename may modify the input
    strncpy(from_inode.name, baseName, MAX_NAME_LENGTH - 1);

    // Write the updated inode back
    write_inode(volume_id, from_inode_index, &from_inode);

    // Note:  doesn't handle updating the parent directory's children list.
    //  need to remove the inode from the old parent's children list and add it to the new parent's.

    return 0; // Success
}

int fs_unlink(const char *path)
{
    printf("unlink\n");

    const char *volume_id = "0"; // Assuming single volume setup

    // Find inode index for the path
    int inode_index = find_inode_index_by_path(volume_id, path);
    if (inode_index < 0)
        return -ENOENT; // File not found

    // Load the inode
    inode target_inode;
    read_inode(volume_id, inode_index, &target_inode);

    if (target_inode.is_directory)
        return -EISDIR; // Target is a directory, should use rmdir

    // Free the data blocks used by the file
    bitmap_t bmp;
    read_bitmap(volume_id, &bmp);
    for (int i = 0; i < target_inode.num_datablocks; i++)
    {
        clear_bit(bmp.datablock_bmp, target_inode.datablocks[i]);
    }
    write_bitmap(volume_id, &bmp);

    // Mark the inode as free
    memset(&target_inode, 0, sizeof(inode));
    write_inode(volume_id, inode_index, &target_inode);

    // Note: doesn't handle updating the parent directory's children list.
    //  need to remove the inode from the parent's children list.

    return 0; // Success
}

struct fuse_operations fs_operations = {
    .getattr = fs_getattr,
    .open = fs_open,
    .readdir = fs_readdir,
    .rename = fs_rename,
    .unlink = fs_unlink,
    .create = fs_create,
    .read = fs_read,
    .write = fs_write,
    .truncate = fs_truncate,
};

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