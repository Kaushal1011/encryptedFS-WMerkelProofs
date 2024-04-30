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

    printf("argc %d\n", argc);

    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d] %s\n", i, argv[i]);
    }

    if (argc < 3)
    {
        printf("Usage: %s <mountpoint> <superblock_path> <key>\n", argv[0]);
        printf("Usage for random keygen: %s keygen <key_path>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "keygen") == 0)
    {
        if (argc < 3)
        {
            printf("Usage: %s keygen <key_path>\n", argv[0]);
            return 1;
        }
        generate_and_store_key(argv[2]);
        return 0;
    }

    // last argument is the key

    extern unsigned char key[crypto_aead_aes256gcm_KEYBYTES];
    load_key(key, argv[argc - 1]);
    printf("key %s\n", key);
    argc--;

    // last second argument is the superblock path

    extern char superblock_path[MAX_PATH_LENGTH];

    // The superblock path is provided as the last argument for simplicity
    strcpy(superblock_path, argv[argc - 1]);

    // Modify the argument list to remove the superblock path
    argc--;

    // Attempt to load the superblock, or create a new one if it doesn't exist
    load_or_create_superblock(superblock_path, &sb);

    // Proceed with FUSE main loop
    return fuse_main(argc, argv, &fs_operations, NULL);
}