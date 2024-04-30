#ifndef BITMAP_H
#define BITMAP_H

#include <stdbool.h>

#include "constants.h"

// bitmap struct to store the bitmap for inode and data block availability
typedef struct bitmap
{
    char inode_bmp[4096];     // Bitmap for inode availability
    char datablock_bmp[4096]; // Bitmap for data block availability
} bitmap_t;

// Function prototypes for bitmap operations
void read_bitmap(char *volume_id, bitmap_t *bmp);
void write_bitmap(char *volume_id, const bitmap_t *bmp);
void set_bit(char *bitmap, int index);
void clear_bit(char *bitmap, int index);
bool is_bit_free(char *bitmap, int index);
int allocate_data_block(bitmap_t *bmp, char *volume_id);

#endif // BITMAP_H
