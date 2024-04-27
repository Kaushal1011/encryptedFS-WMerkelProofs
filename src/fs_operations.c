#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fuse.h>

#include "crypto.h"
#include "fs_operations.h"  

// Define the file system operations here, same as the ones previously in your main file
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
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

int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("read\n");

    (void)fi; // Unused in this simplified example, but can be used for caching inode index etc.

    inode file_inode;
    const char *volume_id = "0";  // Assuming single volume setup for simplicity
    unsigned char key[crypto_aead_aes256gcm_KEYBYTES];
    unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];                               
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

        char encrypted_block_data[BLOCK_SIZE];
        read_volume_block(volume_id, file_inode.datablocks[block_index], encrypted_block_data);

        // Decrypt the data block
        unsigned char decrypted_block_data[BLOCK_SIZE];
        unsigned long long decrypted_block_size;

        if (decrypt_aes_gcm(decrypted_block_data, &decrypted_block_size, (unsigned char *)encrypted_block_data, BLOCK_SIZE, nonce, key) != 0) {
            return -EIO;  // Decryption failed
        }

        // Copy the relevant portion of the decrypted block to the user buffer
        size_t copy_size = MIN(decrypted_block_size - block_offset, bytes_to_read);
        memcpy(buf + bytes_read, decrypted_block_data + block_offset, copy_size);

        bytes_read += bytes_to_read;
        remaining -= bytes_to_read;
        pos += bytes_to_read;
    }

    return bytes_read;
}

int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write\n");

    (void)fi; // Unused in this example

    const char *volume_id = "0"; // Assuming single volume setup
    unsigned char key[crypto_aead_aes256gcm_KEYBYTES];
    unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];
    generate_nonce(nonce); // Generate a new nonce for each operation

    bitmap_t bmp;
    read_bitmap(volume_id, &bmp);

    inode file_inode;
    int inode_index = find_inode_index_by_path(volume_id, path);
    if (inode_index < 0)
        return -ENOENT;

    read_inode(volume_id, inode_index, &file_inode);
    if (file_inode.is_directory)
        return -EISDIR;

    unsigned long long ciphertext_len;
    unsigned char ciphertext[size + crypto_aead_aes256gcm_ABYTES]; // Allocate space for the ciphertext

    // Encrypt the input buffer
    if (encrypt_aes_gcm(ciphertext, &ciphertext_len, (const unsigned char *)buf, size, nonce, key) != 0) {
        return -EIO; // Encryption failed
    }

    size_t bytes_written = 0;
    off_t pos = offset;

    while (bytes_written < ciphertext_len)
    {
        int block_index = pos / BLOCK_SIZE;
        off_t block_offset = pos % BLOCK_SIZE;
        size_t bytes_to_write = MIN(BLOCK_SIZE - block_offset, ciphertext_len - bytes_written);

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

        // Copy encrypted data into the appropriate position in the block
        memcpy(block_data + block_offset, ciphertext + bytes_written, bytes_to_write);
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

int fs_truncate(const char *path, off_t newsize) {
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

int fs_getattr(const char *path, struct stat *stbuf) {
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

int fs_open(const char *path, struct fuse_file_info *fi) {
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

int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
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

int fs_rename(const char *from, const char *to) {
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

int fs_unlink(const char *path) {
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
};