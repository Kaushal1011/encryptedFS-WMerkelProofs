// File: volume.c
#include "volume.h"
#include "bitmap.h"
#include "inode.h"
#include "merkle.h"
#include "constants.h"
#include "crypto.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include "cloud_storage.h"

superblock_t sb;

char superblock_path[MAX_PATH_LENGTH];

char remote_superblock_path[MAX_PATH_LENGTH];

// Initialize a volume with default paths and settings
void init_volume(volume_info_t *volume, const char *path, volume_type type, int volume_id)
{
    snprintf(volume->inodes_path, MAX_PATH_LENGTH, "%sinodes_%d.bin", path, volume_id);
    snprintf(volume->bitmap_path, MAX_PATH_LENGTH, "%sbmp_%d.bin", path, volume_id);
    snprintf(volume->volume_path, MAX_PATH_LENGTH, "%svolume_%d.bin", path, volume_id);
    snprintf(volume->merkle_path, MAX_PATH_LENGTH, "%smerkle_%d.bin", path, volume_id);
    volume->inodes_count = INODES_PER_VOLUME;
    volume->blocks_count = DATA_BLOCKS_PER_VOLUME;
    volume->merkle_tree = NULL;
}

void load_or_create_superblock(const char *path, superblock_t *sb)
{
    FILE *file = fopen(path, "rb+");
    if (!file)
    {
        printf("volume: Superblock file not found, creating a new one.\n");
        file = fopen(path, "wb+");
        for (int i = 0; i < 10; i++)
        {
            init_volume(&sb->volumes[i], "./", LOCAL, i);
        }
        sb->volume_count = 1;
        sb->block_size = BLOCK_SIZE;
        sb->inode_size = sizeof(inode);
        sb->vtype = LOCAL;
        create_volume_files_local(0, sb);
        // Create the root directory inode
        inode root_inode;
        init_inode(&root_inode, "/", S_IFDIR | 0777);
        write_inode(0, &root_inode);
        bitmap_t root_bmp;
        memset(&root_bmp, 0, sizeof(root_bmp));
        set_bit(root_bmp.inode_bmp, 0);
        write_bitmap("0", &root_bmp);
        fwrite(sb, sizeof(superblock_t), 1, file);
    }
    else
    {
        fread(sb, sizeof(superblock_t), 1, file);
        for (int i = 0; i < sb->volume_count; i++)
        {
            sb->volumes[i].merkle_tree = load_merkle_tree_from_file(sb->volumes[i].merkle_path);
        }
    }
    fclose(file);

    printf("volume: Superblock loaded\n");
    printf("volume: Volume count: %d\n", sb->volume_count);
    printf("volume: Block size: %d\n", sb->block_size);
    printf("volume: Inode size: %d\n", sb->inode_size);
    for (int i = 0; i < sb->volume_count; i++)
    {
        printf("volume: Volume %d:\n", i);
        printf("volume: Inodes path: %s\n", sb->volumes[i].inodes_path);
        printf("volume: Bitmap path: %s\n", sb->volumes[i].bitmap_path);
        printf("volume: Volume path: %s\n", sb->volumes[i].volume_path);
        printf("volume: Merkle path: %s\n", sb->volumes[i].merkle_path);
        printf("volume: Inodes count: %d\n", sb->volumes[i].inodes_count);
        printf("volume: Blocks count: %d\n", sb->volumes[i].blocks_count);
    }
}

void load_or_create_remote_superblock(const char *path, superblock_t *sb)
{
    printf("volume: Downloading superblock from remote storage\n");
    // try to download file from google drive

    //  extract directory name from path
    char *directory = strdup(path);

    char *last_slash = strrchr(directory, '/');

    if (last_slash != NULL)
    {
        *last_slash = '\0';
    }

    printf("volume: Directory: %s\n", directory);

    //  extract file name from path
    char *filename = strdup(path);

    if (last_slash != NULL)
    {
        filename = last_slash + 1;
    }

    printf("volume: Filename: %s\n", filename);

    extern OAuthTokens tokens;

    // try to download file from google drive

    int res = download_file_from_folder(directory, filename, &tokens);

    //  read the superblock file and check if it contains 404
    FILE *file = fopen(filename, "r+");

    char *buffer = (char *)malloc(256);
    fread(buffer, 256, 1, file);
    int success = 0;
    if (strstr(buffer, "404") != NULL)
    {
        printf("volume: File not found\n");

        success = 1;
    }

    if (success == 1)
    {
        printf("volume: File not found\n");
        // delete the file
        fclose(file);
        free(buffer);
        remove(filename);
    }

    if (res != CURLE_OK || success == 1)
    {
        printf("volume: Error: Unable to download superblock file from remote storage.\n");
        // create a new superblock with local paths and remote type

        extern char remote_superblock_path[MAX_PATH_LENGTH];

        strcpy(remote_superblock_path, path);

        FILE *file = fopen(filename, "rb+");

        extern char superblock_path[MAX_PATH_LENGTH];

        strcpy(superblock_path, filename);

        if (!file)
        {
            printf("volume: Superblock file not found, creating a new one.\n");
            file = fopen(filename, "wb+");
            for (int i = 0; i < 10; i++)
            {
                init_volume(&sb->volumes[i], "./", GDRIVE, i);
            }
            sb->volume_count = 1;
            sb->block_size = BLOCK_SIZE;
            sb->inode_size = sizeof(inode);
            sb->vtype = GDRIVE;
            create_volume_files_local(0, sb);
            // Create the root directory inode
            inode root_inode;
            init_inode(&root_inode, "/", S_IFDIR | 0777);
            write_inode(0, &root_inode);
            bitmap_t root_bmp;
            memset(&root_bmp, 0, sizeof(root_bmp));
            set_bit(root_bmp.inode_bmp, 0);
            write_bitmap("0", &root_bmp);
            fwrite(sb, sizeof(superblock_t), 1, file);
        }
    }
    else
    {
        extern char remote_superblock_path[MAX_PATH_LENGTH];

        strcpy(remote_superblock_path, path);

        FILE *file = fopen(filename, "rb+");

        extern char superblock_path[MAX_PATH_LENGTH];

        strcpy(superblock_path, filename);

        fread(sb, sizeof(superblock_t), 1, file);

        printf("volume: Superblock loaded\n");

        for (int i = 0; i < sb->volume_count; i++)
        {
            //  download all the volume files
            char *volume_id = (char *)malloc(9);
            sprintf(volume_id, "%d", i);
            char *inodes_path = (char *)malloc(MAX_PATH_LENGTH);
            char *bitmap_path = (char *)malloc(MAX_PATH_LENGTH);
            char *volume_path = (char *)malloc(MAX_PATH_LENGTH);
            char *merkle_path = (char *)malloc(MAX_PATH_LENGTH);

            snprintf(inodes_path, MAX_PATH_LENGTH, "inodes_%s.bin", volume_id);
            snprintf(bitmap_path, MAX_PATH_LENGTH, "bmp_%s.bin", volume_id);
            snprintf(volume_path, MAX_PATH_LENGTH, "volume_%s.bin", volume_id);
            snprintf(merkle_path, MAX_PATH_LENGTH, "merkle_%s.bin", volume_id);

            int res = download_file_from_folder(directory, inodes_path, &tokens);
            if (res != CURLE_OK)
            {
                printf("Error: Unable to download inodes file from remote storage.\n");
            }

            res = download_file_from_folder(directory, bitmap_path, &tokens);
            if (res != CURLE_OK)
            {
                printf("Error: Unable to download bitmap file from remote storage.\n");
            }

            res = download_file_from_folder(directory, volume_path, &tokens);
            if (res != CURLE_OK)
            {
                printf("Error: Unable to download volume file from remote storage.\n");
            }

            res = download_file_from_folder(directory, merkle_path, &tokens);
            if (res != CURLE_OK)
            {
                printf("Error: Unable to download merkle file from remote storage.\n");
            }
        }

        for (int i = 0; i < sb->volume_count; i++)
        {
            sb->volumes[i].merkle_tree = load_merkle_tree_from_file(sb->volumes[i].merkle_path);
        }
    }
}

void create_volume_files_local(int i, superblock_t *sb)
{
    printf("volume: Creating volume files for volume %d\n", i);

    FILE *inodes_file = fopen(sb->volumes[i].inodes_path, "w");
    fclose(inodes_file);
    FILE *bitmap_file = fopen(sb->volumes[i].bitmap_path, "w");
    fclose(bitmap_file);
    FILE *volume_file = fopen(sb->volumes[i].volume_path, "w");
    fclose(volume_file);

    printf("volume: Volume files created for volume %d\n", i);

    FILE *merkle_file = fopen(sb->volumes[i].merkle_path, "wb+");
    if (!merkle_file)
    {
        printf("volume: Merkle file not found, creating a new one.\n");
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
}

void read_volume_block_no_check(int block_index, void *buf)
{
    printf("volume: Reading block %d\n", block_index);
    char volume_id[9] = "0";
    int volume_id_int = block_index / DATA_BLOCKS_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    int block_index_in_volume = block_index % DATA_BLOCKS_PER_VOLUME;

    printf("volume: Reading block %d with volume %d\n", block_index_in_volume, volume_id_int);

    char volume_filename[256];
    sprintf(volume_filename, "volume_%s.bin", volume_id);
    FILE *file = fopen(volume_filename, "rb");

    unsigned char encrypted_data[BLOCK_SIZE + crypto_aead_aes256gcm_ABYTES];
    unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned long long decrypted_len;

    if (file)
    {
        fseek(file, block_index_in_volume * (BLOCK_SIZE + crypto_aead_aes256gcm_ABYTES + sizeof(nonce)), SEEK_SET);
        fread(nonce, sizeof(nonce), 1, file);
        fread(encrypted_data, sizeof(encrypted_data), 1, file);
        fclose(file);
        if (decrypt_aes_gcm(buf, &decrypted_len, encrypted_data, sizeof(encrypted_data), nonce, key) != 0)
        {
            printf("volume: Decryption failed for block %d in volume %s\n", block_index_in_volume, volume_id);
        }
    }
    else
    {
        printf("Error: File %s not found.\n", volume_filename);
    }
}

void read_volume_block(int block_index, void *buf)
{
    printf("volume: Reading block %d\n", block_index);
    char volume_id[9] = "0";
    int volume_id_int = block_index / DATA_BLOCKS_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    read_volume_block_no_check(block_index, buf);

    if (!verify_block_integrity(block_index))
    {
        printf("volume: Integrity check failed for block %d in volume %s\n", block_index, volume_id);
    }
}

void write_volume_block(int block_index, const void *buf, size_t buf_size)
{
    printf("volume: Writing block %d\n", block_index);

    char volume_id[9] = "0";
    int volume_id_int = block_index / DATA_BLOCKS_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    int block_index_in_volume = block_index % DATA_BLOCKS_PER_VOLUME;

    printf("volume: Writing block %d with volume %d\n", block_index_in_volume, volume_id_int);

    char volume_filename[256];
    unsigned char block_buffer[BLOCK_SIZE];
    unsigned char encrypted_data[BLOCK_SIZE + crypto_aead_aes256gcm_ABYTES];
    unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned long long ciphertext_len;

    // Ensure the buffer size does not exceed BLOCK_SIZE
    if (buf_size > BLOCK_SIZE)
    {
        printf("volume: Buffer size exceeds block size. Truncation may occur.\n");
        buf_size = BLOCK_SIZE;
    }

    // Prepare the block buffer with padding
    memcpy(block_buffer, buf, buf_size);
    if (buf_size < BLOCK_SIZE)
    {
        memset(block_buffer + buf_size, 0, BLOCK_SIZE - buf_size); // Zero padding
    }

    // Prepare file and encryption
    sprintf(volume_filename, "volume_%s.bin", volume_id);
    FILE *file = fopen(volume_filename, "r+b");
    generate_nonce(nonce);

    if (file)
    {
        fseek(file, block_index_in_volume * (BLOCK_SIZE + crypto_aead_aes256gcm_ABYTES + sizeof(nonce)), SEEK_SET);
        fwrite(nonce, sizeof(nonce), 1, file);
        if (encrypt_aes_gcm(encrypted_data, &ciphertext_len, block_buffer, BLOCK_SIZE, nonce, key) != 0)
        {
            printf("volume: Encryption failed for block %d in volume %s\n", block_index_in_volume, volume_id);
        }
        fwrite(encrypted_data, BLOCK_SIZE + crypto_aead_aes256gcm_ABYTES, 1, file);
        fclose(file);
    }
    else
    {
        printf("volume: Error: Unable to write to file %s.\n", volume_filename);
    }

    update_merkle_node_for_block(volume_id, block_index_in_volume, block_buffer);
}
// Function to initialize a new superblock
void init_superblock_local(superblock_t *sb)
{
    sb->volume_count = 1; // Start with one volume
    sb->block_size = BLOCK_SIZE;
    sb->inode_size = sizeof(inode);
    // Initialize first volume (Example paths, modify as needed)
    strcpy(sb->volumes[0].inodes_path, "./inodes_0.bin");
    strcpy(sb->volumes[0].bitmap_path, "./bmp_0.bin");
    strcpy(sb->volumes[0].volume_path, "./volume_0.bin");
    sb->volumes[0].inodes_count = INODES_PER_VOLUME;
    sb->volumes[0].blocks_count = DATA_BLOCKS_PER_VOLUME;
}