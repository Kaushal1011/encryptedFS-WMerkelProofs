#include "bitmap.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Read a bitmap from file
void read_bitmap(char *volume_id, bitmap_t *bmp)
{
    char bmp_filename[256];
    sprintf(bmp_filename, "bmp_%s.bin", volume_id);
    FILE *file = fopen(bmp_filename, "rb");
    if (file)
    {
        fread(bmp, sizeof(bitmap_t), 1, file);
        fclose(file);
    }
    else
    {
        perror("Failed to open bitmap file for reading");
    }
}

// Write a bitmap to file
void write_bitmap(char *volume_id, const bitmap_t *bmp)
{
    char bmp_filename[256];
    sprintf(bmp_filename, "bmp_%s.bin", volume_id);
    FILE *file = fopen(bmp_filename, "wb");
    if (file)
    {
        fwrite(bmp, sizeof(bitmap_t), 1, file);
        fclose(file);
    }
    else
    {
        perror("Failed to open bitmap file for writing");
    }
}

// Set a bit in a bitmap
void set_bit(char *bitmap, int index)
{
    int byte_index = index / 8;
    int bit_index = index % 8;
    bitmap[byte_index] |= (1 << bit_index);
}

// Clear a bit in a bitmap
void clear_bit(char *bitmap, int index)
{
    int byte_index = index / 8;
    int bit_index = index % 8;
    bitmap[byte_index] &= ~(1 << bit_index);
}

// Check if a bit is free in a bitmap
bool is_bit_free(char *bitmap, int index)
{
    int byte_index = index / 8;
    int bit_index = index % 8;
    return !(bitmap[byte_index] & (1 << bit_index));
}

int allocate_data_block(bitmap_t *bmp, char *volume_id)
{
    for (int i = 0; i < DATA_BLOCKS_PER_VOLUME; ++i)
    {
        if (is_bit_free(bmp->datablock_bmp, i))
        {
            set_bit(bmp->datablock_bmp, i); // Mark the block as used
            write_bitmap(volume_id, bmp);   // Persist the updated bitmap
            return i;                       // Return the index of the newly allocated block
        }
    }
    return -1; // No free block found
}
