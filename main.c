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

#include "bitmap.h"
#include "fs_operations.h"
#include "constants.h"
#include "inode.h"
#include "volume.h"
#include "crypto.h"
#include <sodium/crypto_aead_aes256gcm.h>
#include <sodium.h>

void add_inode_to_directory(int dir_inode_index, int file_inode_index)
{
}

int main(int argc, char *argv[])
{

    if (sodium_init() == -1)
    {
        return 1; // libsodium didn't initialize properly
    }

    extern superblock_t sb;

    if (argc < 3)
    {
        printf("Key not provided, generating a new one\n");
        extern unsigned char key[crypto_aead_aes256gcm_KEYBYTES];
        generate_and_store_key("key.txt");
        load_key(key, "key.txt");
    }
    else
    {
        extern unsigned char key[crypto_aead_aes256gcm_KEYBYTES];
        load_key(key, argv[argc - 1]);
        printf("key %s\n", key);
        argc--;
    }

    // Check if a superblock path is provided as an argument
    if (argc < 3)
    {
        printf("Usage: %s <mountpoint> <superblock_path> <key>\n", argv[0]);
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