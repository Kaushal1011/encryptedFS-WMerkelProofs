#ifndef BITMAP_H
#define BITMAP_H

#include <stdbool.h>

#include "constants.h"

// Define the bitmap struct
typedef struct bitmap {
    char inode_bmp[4096];  // Bitmap for inode availability
    char datablock_bmp[4096];  // Bitmap for data block availability
} bitmap_t;

// Function prototypes for bitmap operations
void read_bitmap(const char *volume_id, bitmap_t *bmp);
void write_bitmap(const char *volume_id, const bitmap_t *bmp);
void set_bit(unsigned char *bitmap, int index);
void clear_bit(unsigned char *bitmap, int index);
bool is_bit_free(unsigned char *bitmap, int index);
int allocate_data_block(bitmap_t *bmp, const char *volume_id);

#endif // BITMAP_H
