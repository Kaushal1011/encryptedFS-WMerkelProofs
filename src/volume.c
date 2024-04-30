#include "volume.h"
#include "bitmap.h"
#include "inode.h"
#include "merkle.h"
#include "constants.h"
#include "crypto.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

superblock_t sb;

char superblock_path[MAX_PATH_LENGTH];

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
        printf("Superblock file not found, creating a new one.\n");
        file = fopen(path, "wb+");
        for (int i = 0; i < 10; i++)
        {
            init_volume(&sb->volumes[i], "./", LOCAL, i);
        }
        sb->volume_count = 1;
        sb->block_size = BLOCK_SIZE;
        sb->inode_size = sizeof(inode);
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

    printf("Superblock loaded\n");
    printf("Volume count: %d\n", sb->volume_count);
    printf("Block size: %d\n", sb->block_size);
    printf("Inode size: %d\n", sb->inode_size);
    for (int i = 0; i < sb->volume_count; i++)
    {
        printf("Volume %d:\n", i);
        printf("Inodes path: %s\n", sb->volumes[i].inodes_path);
        printf("Bitmap path: %s\n", sb->volumes[i].bitmap_path);
        printf("Volume path: %s\n", sb->volumes[i].volume_path);
        printf("Merkle path: %s\n", sb->volumes[i].merkle_path);
        printf("Inodes count: %d\n", sb->volumes[i].inodes_count);
        printf("Blocks count: %d\n", sb->volumes[i].blocks_count);
    }
}

void create_volume_files_local(int i, superblock_t *sb)
{
    printf("Creating volume files for volume %d\n", i);

    FILE *inodes_file = fopen(sb->volumes[i].inodes_path, "w");
    fclose(inodes_file);
    FILE *bitmap_file = fopen(sb->volumes[i].bitmap_path, "w");
    fclose(bitmap_file);
    FILE *volume_file = fopen(sb->volumes[i].volume_path, "w");
    fclose(volume_file);

    printf("Volume files created for volume %d\n", i);

    FILE *merkle_file = fopen(sb->volumes[i].merkle_path, "wb+");
    if (!merkle_file)
    {
        printf("Merkle file not found, creating a new one.\n");
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
    char volume_id[9] = "0";
    int volume_id_int = block_index / DATA_BLOCKS_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    int block_index_in_volume = block_index % DATA_BLOCKS_PER_VOLUME;

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
            printf("Decryption failed for block %d in volume %s\n", block_index_in_volume, volume_id);
        }
    }
    else
    {
        printf("Error: File %s not found.\n", volume_filename);
    }
}

void read_volume_block(int block_index, void *buf)
{
    char volume_id[9] = "0";
    int volume_id_int = block_index / DATA_BLOCKS_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    read_volume_block_no_check(block_index, buf);

    if (!verify_block_integrity(block_index))
    {
        printf("Integrity check failed for block %d in volume %s\n", block_index, volume_id);
    }
}

void write_volume_block(int block_index, const void *buf, size_t buf_size)
{
    printf("Writing block %d\n", block_index);

    char volume_id[9] = "0";
    int volume_id_int = block_index / DATA_BLOCKS_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    int block_index_in_volume = block_index % DATA_BLOCKS_PER_VOLUME;

    printf("Writing block %d with volume %d\n", block_index_in_volume, volume_id_int);

    char volume_filename[256];
    unsigned char block_buffer[BLOCK_SIZE];
    unsigned char encrypted_data[BLOCK_SIZE + crypto_aead_aes256gcm_ABYTES];
    unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned long long ciphertext_len;

    // Ensure the buffer size does not exceed BLOCK_SIZE
    if (buf_size > BLOCK_SIZE)
    {
        printf("Buffer size exceeds block size. Truncation may occur.\n");
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
            printf("Encryption failed for block %d in volume %s\n", block_index_in_volume, volume_id);
        }
        fwrite(encrypted_data, BLOCK_SIZE + crypto_aead_aes256gcm_ABYTES, 1, file);
        fclose(file);
    }
    else
    {
        printf("Error: Unable to write to file %s.\n", volume_filename);
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