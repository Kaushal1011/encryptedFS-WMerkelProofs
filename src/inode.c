#include "inode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
// Read an inode from file
void read_inode(const char *volume_id, int inode_index, inode *inode_buf) {
    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);
    FILE *file = fopen(inode_filename, "rb");
    if (file) {
        fseek(file, inode_index * sizeof(inode), SEEK_SET);
        fread(inode_buf, sizeof(inode), 1, file);
        fclose(file);
    } else {
        perror("Failed to open inode file for reading");
    }
}

// Write an inode to file
void write_inode(const char *volume_id, int inode_index, const inode *inode_buf) {
    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);
    FILE *file = fopen(inode_filename, "r+b");
    if (file) {
        fseek(file, inode_index * sizeof(inode), SEEK_SET);
        fwrite(inode_buf, sizeof(inode), 1, file);
        fclose(file);
    } else {
        perror("Failed to open inode file for writing");
    }
}

// Initialize an inode with default values
void init_inode(inode *node, const char *path, mode_t mode) {
    memset(node, 0, sizeof(inode));
    node->valid = 1;
    strncpy(node->path, path, MAX_PATH_LENGTH);
    node->path[MAX_PATH_LENGTH - 1] = '\0';
    char *basename_str = basename(strdup(path));
    strncpy(node->name, basename_str, MAX_NAME_LENGTH);
    node->name[MAX_NAME_LENGTH - 1] = '\0';
    free(basename_str);
    node->permissions = mode;
    node->is_directory = S_ISDIR(mode);
    node->user_id = getuid();
    node->group_id = getgid();
    time_t now = time(NULL);
    node->a_time = now;
    node->m_time = now;
    node->c_time = now;
    node->b_time = now;
    node->size = 0;
    node->num_datablocks = 0;
    for (int i = 0; i < MAX_DATABLOCKS; i++) {
        node->datablocks[i] = -1;
    }
    node->num_children = 0;
    node->parent_inode = -1;
    node->num_links = 1;
}
