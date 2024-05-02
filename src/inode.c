// File: inode.c
#include "inode.h"
#include "crypto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
// Read an inode from file
void read_inode(int inode_index, inode *inode_buf)
{
    // if inode index is greater than the total number of inodes,
    //  we handle it to read the inode from the next volume
    printf("inode: Reading inode %d\n", inode_index);
    char volume_id[9] = "0";

    int volume_id_int = inode_index / INODES_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    int inode_index_in_volume = inode_index % INODES_PER_VOLUME;

    printf("inode: Reading inode index in volume %d\n", inode_index_in_volume);
    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);
    FILE *file = fopen(inode_filename, "rb");
    if (file)
    {
        unsigned char encrypted_data[sizeof(inode) + crypto_aead_aes256gcm_ABYTES];
        unsigned long long decrypted_len;
        unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];
        extern unsigned char key[crypto_aead_aes256gcm_KEYBYTES];
        fseek(file, inode_index_in_volume * (sizeof(inode) + sizeof(nonce) + crypto_aead_aes256gcm_ABYTES), SEEK_SET);
        fread(nonce, sizeof(nonce), 1, file);
        fread(encrypted_data, sizeof(encrypted_data), 1, file);

        if (decrypt_aes_gcm((unsigned char *)inode_buf, &decrypted_len, encrypted_data, sizeof(encrypted_data), nonce, key) != 0)
        {
            perror("Failed to decrypt inode");
            fclose(file);
            return;
        }

        fclose(file);
    }
    else
    {
        perror("Failed to open inode file for reading");
    }
}

// Write an inode to file
void write_inode(int inode_index, const inode *inode_buf)
{
    // if inode index is greater than the total number of inodes,
    //  we handle it to write the inode to the next volume

    printf("inode: Writing inode %d\n", inode_index);
    char volume_id[9] = "0";

    int volume_id_int = inode_index / INODES_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    int inode_index_in_volume = inode_index % INODES_PER_VOLUME;

    printf("inode: Writing inode in volume %d\n", volume_id_int);

    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);

    printf("inode: Writing inode %d\n", inode_index_in_volume);
    FILE *file = fopen(inode_filename, "r+b");
    if (file)
    {
        unsigned char encrypted_data[sizeof(inode) + crypto_aead_aes256gcm_ABYTES];
        unsigned long long ciphertext_len;
        unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];
        extern unsigned char key[crypto_aead_aes256gcm_KEYBYTES];

        printf("key %s\n", key);

        generate_nonce(nonce);
        int retFlag = encrypt_aes_gcm(encrypted_data, &ciphertext_len, (unsigned char *)inode_buf, sizeof(inode), nonce, key);

        printf("retFlag %d\n", retFlag);

        if (encrypt_aes_gcm(encrypted_data, &ciphertext_len, (unsigned char *)inode_buf, sizeof(inode), nonce, key) != 0)
        {
            perror("Failed to encrypt inode");
            fclose(file);
            return;
        }

        fseek(file, inode_index_in_volume * (sizeof(inode) + sizeof(nonce) + crypto_aead_aes256gcm_ABYTES), SEEK_SET);
        fwrite(nonce, sizeof(nonce), 1, file);
        fwrite(encrypted_data, ciphertext_len, 1, file);
        fclose(file);
    }
    else
    {
        perror("Failed to open inode file for writing");
    }
}

// Initialize an inode with default values
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

    for (int i = 0; i < MAX_CHILDREN; ++i)
    {
        node->children[i] = -1; // Initialize all children indices to -1 indicating they are not used
    }

    // Initialize encryption-related fields if necessary

    // Initialize the file type and link count
    node->type[0] = '\0'; // Assuming the type needs to be determined elsewhere or is not applicable
    node->num_links = 1;  // A newly created file typically has one link
}

int allocate_inode_bmp(bitmap_t *bmp, char *volume_id)
{
    printf("inode: Allocating inode bitmap for %s\n", volume_id);

    //  for safety reasons, we will not allocate the first inode
    for (int i = 1; i < INODES_PER_VOLUME; ++i)
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

// temporary implementation
int find_inode_index_by_path(const char *target_path)
{

    inode root;
    // read the root inode
    read_inode(0, &root);

    // check if the root inode is the target
    if (strcmp(root.path, target_path) == 0)
    {
        printf("inode: Root inode is the target %s %s \n", root.path, target_path);
        return 0;
    }

    inode temp_inode;

    for (int i = 0; i < root.num_children; i++)
    {
        read_inode(root.children[i], &temp_inode);
        if (strcmp(temp_inode.path, target_path) == 0)
        {
            return root.children[i];
        }
    }

    return -1; // Target path not found
}