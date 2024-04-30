#ifndef INODE_H
#define INODE_H

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>

#include "constants.h"
#include "bitmap.h"

#define MAX_PATH_LENGTH 256
#define MAX_NAME_LENGTH 256
#define MAX_TYPE_LENGTH 20
#define ENCRYPTION_KEY_SIZE 32
#define AES_GCM_NONCE_SIZE 12
#define AES_GCM_TAG_SIZE 16

// Define the inode structure
typedef struct inode
{
    int valid;                      // 0 if the inode is not valid, 1 if it is
    int inode_number;               // The inode number (Not used rn, but might be useful later)
    char path[MAX_PATH_LENGTH];     // The path of the file
    char name[MAX_NAME_LENGTH];     // The name of the file
    mode_t permissions;             // The permissions of the file
    bool is_directory;              // True if the inode is a directory, false if it is a file
    uid_t user_id;                  // The user id of the owner
    gid_t group_id;                 // The group id of the owner
    time_t a_time;                  // The last access time
    time_t m_time;                  // The last modification time
    time_t c_time;                  // The creation time
    time_t b_time;                  // The last time the inode was modified
    off_t size;                     // The size of the file
    int datablocks[MAX_DATABLOCKS]; // The data blocks that the file is stored in
    int num_datablocks;             // The number of data blocks that the file is stored in
    int parent_inode;               // The inode number of the parent directory
    int children[MAX_CHILDREN];     // Make sure MAX_CHILDREN is defined somewhere
    int num_children;               // The number of children in the directory
    char type[MAX_TYPE_LENGTH];     // The type of the file
    int num_links;                  // The number of links to the file
} inode;

// Function prototypes for inode operations
void read_inode(int inode_index, inode *inode_buf);
void write_inode(int inode_index, const inode *inode_buf);
void init_inode(inode *node, const char *path, mode_t mode);

int allocate_inode_bmp(bitmap_t *bmp, char *volume_id);
int find_inode_index_by_path(const char *target_path);

#endif // INODE_H
