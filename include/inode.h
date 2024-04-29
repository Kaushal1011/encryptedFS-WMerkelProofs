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
    int valid;
    int inode_number;
    char path[MAX_PATH_LENGTH];
    char name[MAX_NAME_LENGTH];
    mode_t permissions;
    bool is_directory;
    uid_t user_id;
    gid_t group_id;
    time_t a_time;
    time_t m_time;
    time_t c_time;
    time_t b_time;
    off_t size;
    unsigned char encryption_key[ENCRYPTION_KEY_SIZE];
    unsigned char nonce[AES_GCM_NONCE_SIZE];
    unsigned char auth_tag[AES_GCM_TAG_SIZE];
    int datablocks[MAX_DATABLOCKS];
    int num_datablocks;
    int parent_inode;
    int children[MAX_CHILDREN]; // Make sure MAX_CHILDREN is defined somewhere
    int num_children;
    char type[MAX_TYPE_LENGTH];
    int num_links;
} inode;

// Function prototypes for inode operations
void read_inode(char *volume_id, int inode_index, inode *inode_buf);
void write_inode(char *volume_id, int inode_index, const inode *inode_buf);
void init_inode(inode *node, const char *path, mode_t mode);

int allocate_inode_bmp(bitmap_t *bmp, char *volume_id);
int find_inode_index_by_path(char *volume_id, const char *target_path);

#endif // INODE_H
