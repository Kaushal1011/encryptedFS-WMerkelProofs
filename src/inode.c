#include "inode.h"
#include "crypto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
void read_inode(char *volume_id, int inode_index, inode *inode_buf)
{
    printf("Reading inode %d\n", inode_index);
    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);
    FILE *file = fopen(inode_filename, "rb");
    if (file)
    {
        unsigned char encrypted_data[sizeof(inode) + crypto_aead_chacha20poly1305_IETF_ABYTES];
        unsigned long long decrypted_len;
        unsigned char nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES];
        extern unsigned char key[crypto_aead_chacha20poly1305_IETF_KEYBYTES];
        fseek(file, inode_index * (sizeof(inode) + sizeof(nonce) + crypto_aead_chacha20poly1305_IETF_ABYTES), SEEK_SET);
        fread(nonce, sizeof(nonce), 1, file);
        fread(encrypted_data, sizeof(encrypted_data), 1, file);

        if (decrypt_data((unsigned char *)inode_buf, &decrypted_len, encrypted_data, sizeof(encrypted_data), nonce, key) != 0)
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

void write_inode(char *volume_id, int inode_index, const inode *inode_buf)
{
    printf("Writing inode %d\n", inode_index);
    char inode_filename[256];
    sprintf(inode_filename, "inodes_%s.bin", volume_id);
    FILE *file = fopen(inode_filename, "r+b");
    if (file)
    {
        unsigned char encrypted_data[sizeof(inode) + crypto_aead_chacha20poly1305_IETF_ABYTES];
        unsigned long long ciphertext_len;
        unsigned char nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES];
        extern unsigned char key[crypto_aead_chacha20poly1305_IETF_KEYBYTES];

        randombytes_buf(nonce, sizeof(nonce)); // libsodium random nonce generation
        if (encrypt_data(encrypted_data, &ciphertext_len, (unsigned char *)inode_buf, sizeof(inode), nonce, key) != 0)
        {
            perror("Failed to encrypt inode");
            fclose(file);
            return;
        }

        fseek(file, inode_index * (sizeof(inode) + sizeof(nonce) + crypto_aead_chacha20poly1305_IETF_ABYTES), SEEK_SET);
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

    // Initialize encryption-related fields if necessary

    // Initialize the file type and link count
    node->type[0] = '\0'; // Assuming the type needs to be determined elsewhere or is not applicable
    node->num_links = 1;  // A newly created file typically has one link
}

int allocate_inode_bmp(bitmap_t *bmp, char *volume_id)
{
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

// temporary implementation
int find_inode_index_by_path(char *volume_id, const char *target_path)
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

    for (int i = 0; i < INODES_PER_VOLUME; i++)
    {
        read_inode(volume_id, i, &temp_inode);
        if (strcmp(temp_inode.path, target_path) == 0)
        {
            fclose(file);
            return i;
        }
    }

    fclose(file);
    return -1; // Target path not found
}