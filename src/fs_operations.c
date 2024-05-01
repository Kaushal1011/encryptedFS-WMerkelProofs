#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fuse.h>
#include <libgen.h>
#include <curl/curl.h>

#include "fs_operations.h"
#include "volume.h"
#include "cloud_storage.h"

// function pointer type def for allocation functions
typedef int (*alloc_func)(bitmap_t *bmp, char *volume_id);

// Find the index of a free inode/datablock in the file system
int manage_volume_allocation(superblock_t *sb, char *volume_id, void *bmp, alloc_func funcPoint)
{
    char volume_id_new[9];
    strcpy(volume_id_new, volume_id); // Copy current volume_id to volume_id_new
    int inode_index = funcPoint(bmp, volume_id_new);
    int volume_num = atoi(volume_id_new) + 1;

    // load new volumes bitmap and check for free inode
    bitmap_t bmp_new;

    while (inode_index == -1)
    {
        printf("Expanding volume SEarCH\n");

        printf("volume_num: %d\n", volume_num);

        if (volume_num == NUMVOLUMES - 1)
        {
            return -1; // No space left for new inode
        }

        if (volume_num < sb->volume_count)
        {
            sprintf(volume_id_new, "%d", volume_num);
            read_bitmap(volume_id_new, &bmp_new);
            inode_index = funcPoint(&bmp_new, volume_id_new);
            printf("inode_index: %d\n", inode_index);
            printf("volume_id_new: %s\n", volume_id_new);
            if (inode_index != -1)
            {
                strcpy(volume_id, volume_id_new);
                break;
            }
            volume_num = volume_num + 1;
        }
        else
        {
            // Init new volume if not init
            sb->volume_count = sb->volume_count + 1;
            printf("volume_num: %d\n", volume_num);
            printf("sb->volume_count: %d\n", sb->volume_count);
            printf("created new volume file");
            create_volume_files_local(volume_num, sb);
            sprintf(volume_id_new, "%d", volume_num);
            bitmap_t bmp_new;
            memset(&bmp_new, 0, sizeof(bmp));
            //  set inode 0 as used data node 0 as used to avoid overwriting root inode
            set_bit(bmp_new.inode_bmp, 0);     // never used for expansion safety 0*(volid) = 0
            set_bit(bmp_new.datablock_bmp, 0); // never used for expansion safety
            write_bitmap(volume_id_new, &bmp_new);
            inode_index = funcPoint(&bmp_new, volume_id_new);
            printf("inode_index: %d\n", inode_index);
            //  store superblock
            extern char superblock_path[MAX_PATH_LENGTH];
            FILE *file = fopen(superblock_path, "wb+");
            if (file)
            {
                fwrite(sb, sizeof(superblock_t), 1, file);
                fclose(file);
            }
            if (inode_index != -1)
            {
                strcpy(volume_id, volume_id_new);
                break;
            }
            volume_num = volume_num + 1;
        }
    }
    return inode_index;
}

// Define the file system operations here, same as the ones previously in your main file
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    printf("in create\n");

    (void)fi; // The fuse_file_info is not used in this simple example

    char volume_id[9] = "0";

    // Load the current bitmap to find a free inode
    bitmap_t bmp;
    read_bitmap(volume_id, &bmp);

    // Allocate a new inode for the file
    int inode_index = manage_volume_allocation(&sb, volume_id, &bmp, allocate_inode_bmp);

    inode_index = inode_index + INODES_PER_VOLUME * atoi(volume_id);

    printf("inode_index after volume adjust: %d\n", inode_index);

    if (inode_index == -1)
    {
        return -ENOSPC; // No space left for new inode
    }

    printf("inode_index: %d\n", inode_index);

    // Initialize the new inode
    inode new_inode;
    init_inode(&new_inode, path, mode);

    // Write the new inode to the inode file
    write_inode(inode_index, &new_inode);

    inode root_inode;
    read_inode(0, &root_inode); // root inode is at index 0

    if (root_inode.num_children < MAX_CHILDREN)
    {
        root_inode.children[root_inode.num_children++] = inode_index;
        write_inode(0, &root_inode); // Update root inode
    }
    else
    {
        return -ENOSPC; // No space left
    }

    return 0; // Success
}

int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("read\n");

    (void)fi;

    inode file_inode;
    int inode_index = find_inode_index_by_path(path);

    if (inode_index < 0)
    {
        return -ENOENT; // No such file
    }

    // Load the inode information
    read_inode(inode_index, &file_inode);

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
        //  determine volume_id based on file_inode.datablocks[block_index]
        read_volume_block(file_inode.datablocks[block_index], block_data);

        memcpy(buf + bytes_read, block_data + block_offset, bytes_to_read);

        bytes_read += bytes_to_read;
        remaining -= bytes_to_read;
        pos += bytes_to_read;
    }

    return bytes_read;
}

int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("write\n");

    (void)fi;

    char volume_id[9] = "0"; // managed by later functions
    bitmap_t bmp;
    read_bitmap(volume_id, &bmp);

    inode file_inode;
    int inode_index = find_inode_index_by_path(path);

    // handles checking whatever volume is needed to be checked

    if (inode_index < 0)
        return -ENOENT;

    read_inode(inode_index, &file_inode);
    if (file_inode.is_directory)
        return -EISDIR;

    size_t bytes_written = 0;
    off_t pos = offset;

    char volume_id_datablocks[9] = "0";
    while (bytes_written < size)
    {
        int block_index = pos / BLOCK_SIZE;
        off_t block_offset = pos % BLOCK_SIZE;
        size_t bytes_to_write = MIN(BLOCK_SIZE - block_offset, size - bytes_written);

        int allocate_new_block = 0;

        if (block_index >= file_inode.num_datablocks)
        {
            // Allocate a new block, if volume_id is not enough for new block, allocate new volume
            int new_block_index = manage_volume_allocation(&sb, volume_id_datablocks, &bmp, allocate_data_block);

            printf("new_block_index: %d\n", new_block_index);
            printf("volume_id_datablocks: %s\n", volume_id_datablocks);

            allocate_new_block = 1;

            if (new_block_index == -1)
                return -ENOSPC; // No space left

            // datablock index is stored as volume_id * DATA_BLOCKS_PER_VOLUME + block_index it is handled in write and read functions
            file_inode.datablocks[block_index] = new_block_index + DATA_BLOCKS_PER_VOLUME * atoi(volume_id_datablocks);
            file_inode.num_datablocks += 1;
        }

        char block_data[BLOCK_SIZE] = {0};
        // Read existing block data if not writing a full block
        if (bytes_to_write < BLOCK_SIZE && allocate_new_block == 0)
        {
            //  while reading determine volume_id based on file_inode.datablocks[block_index]
            read_volume_block_no_check(file_inode.datablocks[block_index], block_data);
        }

        // Copy data to block
        memcpy(block_data + block_offset, buf + bytes_written, bytes_to_write);
        write_volume_block(file_inode.datablocks[block_index], block_data, BLOCK_SIZE);

        bytes_written += bytes_to_write;
        pos += bytes_to_write;
    }

    // Update file size
    if (offset + size > file_inode.size)
    {
        file_inode.size = offset + size;
    }
    printf("file_inode.size: %ld\n", file_inode.size);
    write_inode(inode_index, &file_inode);

    return bytes_written;
}

// okay checking volumes here and adding logic might be tough
int fs_truncate(const char *path, off_t newsize)
{
    printf("truncate\n");

    char volume_id[9] = "0";
    int inode_index = find_inode_index_by_path(path);

    //  supposed to work for all volumes

    if (inode_index < 0)
    {
        return -ENOENT; // File not found
    }

    inode file_inode;
    read_inode(inode_index, &file_inode);

    if (file_inode.is_directory)
    {
        return -EISDIR; // Cannot truncate a directory
    }

    bitmap_t bmp;
    if (newsize < file_inode.size)
    {
        // Calculate the number of blocks needed after truncation
        int new_blocks_needed = (newsize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        // Free blocks beyond the new size
        for (int i = new_blocks_needed; i < file_inode.num_datablocks; i++)
        {
            //  determine volume_id based on file_inode.datablocks[block_index]
            char volume_id_datablocks[9] = "0";
            int volume_index = file_inode.datablocks[i] / DATA_BLOCKS_PER_VOLUME;
            sprintf(volume_id_datablocks, "%d", volume_index);
            read_bitmap(volume_id_datablocks, &bmp);
            clear_bit(bmp.datablock_bmp, file_inode.datablocks[i] % DATA_BLOCKS_PER_VOLUME);
            write_bitmap(volume_id_datablocks, &bmp);
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
            int new_block_index = manage_volume_allocation(&sb, volume_id, &bmp, allocate_data_block);
            if (new_block_index == -1)
                return -ENOSPC; // No space left for new blocks

            file_inode.datablocks[i] = new_block_index + DATA_BLOCKS_PER_VOLUME * atoi(volume_id);
            file_inode.num_datablocks += 1;

            // Initialize the new block to zero
            char zero_block[BLOCK_SIZE] = {0};
            write_volume_block(new_block_index, zero_block, BLOCK_SIZE);
        }
    }

    // Update the inode size and write back
    file_inode.size = newsize;
    write_inode(inode_index, &file_inode);

    return 0; // Success
}

int fs_getattr(const char *path, struct stat *stbuf)
{
    printf("getattr\n");

    printf("path: %s\n", path);

    memset(stbuf, 0, sizeof(struct stat)); // Clear the stat buffer

    int inode_index = find_inode_index_by_path(path);

    if (inode_index == -1)
        return -ENOENT;

    inode node;
    read_inode(inode_index, &node);
    if (!node.valid)
        return -ENOENT;

    stbuf->st_ino = inode_index;
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

    int inode_index = find_inode_index_by_path(path);
    // handle checking for file
    if (inode_index == -1)
        return -ENOENT; // No such file

    inode file_inode;
    read_inode(inode_index, &file_inode);

    // Check if directory (directories cannot be opened)
    if (file_inode.is_directory)
        return -EISDIR;

    return 0;
}

int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("readdir\n");

    (void)offset; // Not used in this function
    (void)fi;     // Not used in this function

    // int dir_inode_index = find_inode_index_by_path( path)
    int dir_inode_index = 0; // Assuming root directory for now

    if (dir_inode_index < 0)
    {
        return -ENOENT; // Directory not found
    }

    inode dir_inode;
    read_inode(dir_inode_index, &dir_inode);

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

    //  reading across volumes should be supported here
    // List child inodes
    for (int i = 0; i < dir_inode.num_children; i++)
    {
        if (dir_inode.children[i] == -1)
        {
            continue; // Skip uninitialized entries
        }

        inode child_inode;
        read_inode(dir_inode.children[i], &child_inode);

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

    // Find inode index for the source path
    int from_inode_index = find_inode_index_by_path(from);
    if (from_inode_index < 0)
        return -ENOENT; // Source not found

    inode from_inode;
    read_inode(from_inode_index, &from_inode);

    // Check if the target exists
    int to_inode_index = find_inode_index_by_path(to);
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
    write_inode(from_inode_index, &from_inode);

    // Note:  doesn't handle updating the parent directory's children list.
    //  need to remove the inode from the old parent's children list and add it to the new parent's.

    return 0; // Success
}

int fs_unlink(const char *path)
{
    // handle mutli volume setup
    // also clear data blocks for the deleted inode in volume handled setup
    printf("unlink\n");

    char volume_id[9] = "0";

    // Find inode index for the path
    int inode_index = find_inode_index_by_path(path);
    if (inode_index < 0)
        return -ENOENT; // File not found

    // Load the inode
    inode target_inode;
    read_inode(inode_index, &target_inode);

    if (target_inode.is_directory)
        return -EISDIR; // Target is a directory, should use rmdir

    // Free the data blocks used by the file
    bitmap_t bmp;
    for (int i = 0; i < target_inode.num_datablocks; i++)
    {
        //  determine volume_id based on file_inode.datablocks[block_index]
        int volume_index = target_inode.datablocks[i] / DATA_BLOCKS_PER_VOLUME;
        char volume_id_datablocks[9] = "0";
        sprintf(volume_id_datablocks, "%d", volume_index);
        read_bitmap(volume_id_datablocks, &bmp);
        clear_bit(bmp.datablock_bmp, target_inode.datablocks[i] % DATA_BLOCKS_PER_VOLUME);
    }
    write_bitmap(volume_id, &bmp);

    // Mark the inode as free
    memset(&target_inode, 0, sizeof(inode));
    write_inode(inode_index, &target_inode);

    // Note: doesn't handle updating the parent directory's children list.
    //  need to remove the inode from the parent's children list.

    return 0; // Success
}

void fs_destroy()
{
    printf("destroy\n");
    extern superblock_t sb;

    extern char superblock_path[MAX_PATH_LENGTH];

    extern char remote_superblock_path[MAX_PATH_LENGTH];

    if (sb.vtype == GDRIVE)
    {
        printf("uploading to remote\n");

        char foldername[MAX_PATH_LENGTH];
        strcpy(foldername, remote_superblock_path);

        char *last_slash = strrchr(foldername, '/');

        if (last_slash != NULL)
        {
            *last_slash = '\0';
        }

        for (int i = 0; i < sb.volume_count; i++)
        {
            extern OAuthTokens tokens;
            // upload bmp file
            char volume_id[9];
            sprintf(volume_id, "%d", i);
            char *bmp_path = strrchr(sb.volumes[i].bitmap_path, '/') + 1;
            if (upload_file_to_folder(foldername, bmp_path, &tokens) != CURLE_OK)
            {
                perror("upload failed bmp");
            }

            char *inodes_path = strrchr(sb.volumes[i].inodes_path, '/') + 1;
            // upload inode file
            if (upload_file_to_folder(foldername, inodes_path, &tokens) != CURLE_OK)
            {
                perror("upload failed inode");
            }

            char *volume_path = strrchr(sb.volumes[i].volume_path, '/') + 1;
            // upload data file
            if (upload_file_to_folder(foldername, volume_path, &tokens) != CURLE_OK)
            {
                perror("upload failed data");
            }

            char *merkle_path = strrchr(sb.volumes[i].merkle_path, '/') + 1;
            // upload merkle file
            if (upload_file_to_folder(foldername, merkle_path, &tokens) != CURLE_OK)
            {
                perror("upload failed merkle");
            }
        }

        // finally upload the superblock

        if (upload_file_to_folder(foldername, superblock_path, &tokens) != CURLE_OK)
        {
            perror("upload failed superblock");
        }
    }

    return;
}

const struct fuse_operations fs_operations = {
    .getattr = fs_getattr,
    .open = fs_open,
    .readdir = fs_readdir,
    .rename = fs_rename,
    .unlink = fs_unlink,
    .create = fs_create,
    .read = fs_read,
    .write = fs_write,
    .truncate = fs_truncate,
    .destroy = fs_destroy,
};