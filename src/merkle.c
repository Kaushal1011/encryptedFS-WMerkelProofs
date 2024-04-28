#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/sha.h>

#include "merkle.h"
#include "constants.h"

MerkleNode *create_merkle_node(char *hash, MerkleNode *left, MerkleNode *right, int block_index)
{
    MerkleNode *node = malloc(sizeof(MerkleNode));
    if (node)
    {
        memcpy(node->hash, hash, 32);
        node->left = left;
        node->right = right;
        node->parent = NULL; // Will be set during tree construction
        node->block_index = block_index;
    }
    return node;
}

void compute_hash(const char *input, char *output)
{
    unsigned char temp_hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, strlen(input), temp_hash);
    memcpy(output, temp_hash, 32);
}

bool compare_hashes(const char *hash1, const char *hash2)
{
    return memcmp(hash1, hash2, 32) == 0;
}

void update_merkle_node(MerkleNode *node, const char *new_hash)
{
    memcpy(node->hash, new_hash, 32);
    while (node->parent)
    {
        char concat_hash[64];
        if (node->parent->left && node->parent->right)
        {
            memcpy(concat_hash, node->parent->left->hash, 32);
            memcpy(concat_hash + 32, node->parent->right->hash, 32);
        }
        else
        {
            memcpy(concat_hash, node->hash, 32); // Single child scenario
        }
        compute_hash(concat_hash, node->parent->hash);
        node = node->parent;
    }
}

#include <math.h> // Include math for using log2 function, if needed

MerkleTree *build_merkle_tree(char **block_hashes, int num_blocks)
{
    if (num_blocks == 0)
    {
        return NULL;
    }

    MerkleNode **current_level = malloc(num_blocks * sizeof(MerkleNode *));
    for (int i = 0; i < num_blocks; i++)
    {
        current_level[i] = create_merkle_node(block_hashes[i], NULL, NULL, i);
    }

    int num_nodes = num_blocks;
    int depth = 0; // Initialize depth to zero

    while (num_nodes > 1)
    {
        int next_level_size = (num_nodes + 1) / 2;
        MerkleNode **next_level = malloc(next_level_size * sizeof(MerkleNode *));
        depth++; // Each iteration represents building a new level in the tree

        for (int i = 0; i < num_nodes / 2; i++)
        {
            char combined_hash[64];
            memcpy(combined_hash, current_level[2 * i]->hash, 32);
            memcpy(combined_hash + 32, current_level[2 * i + 1]->hash, 32);
            char new_hash[32];
            compute_hash(combined_hash, new_hash);
            // block index is set to -1 for intermediate nodes
            next_level[i] = create_merkle_node(new_hash, current_level[2 * i], current_level[2 * i + 1], -1);
            current_level[2 * i]->parent = next_level[i];
            current_level[2 * i + 1]->parent = next_level[i];
        }

        if (num_nodes % 2 == 1)
        {
            next_level[next_level_size - 1] = current_level[num_nodes - 1];
            current_level[num_nodes - 1]->parent = next_level[next_level_size - 1];
        }

        free(current_level);
        current_level = next_level;
        num_nodes = next_level_size;
    }

    MerkleTree *tree = malloc(sizeof(MerkleTree));
    if (!tree)
    {
        free(current_level); // Ensure to free memory in case of allocation failure
        return NULL;
    }
    tree->root = current_level[0];
    tree->depth = depth; // Set the depth of the tree

    free(current_level);
    return tree;
}

bool verify_merkle_path(MerkleNode *leaf_node, const char *expected_root_hash, char decrypted_hash[32])
{
    MerkleNode *current = leaf_node;
    char current_hash[32] = {0};
    memcpy(current_hash, decrypted_hash, 32);

    while (current->parent)
    {
        MerkleNode *sibling = (current == current->parent->left) ? current->parent->right : current->parent->left;
        char concat_hash[64];
        if (current == current->parent->left && sibling)
        {
            memcpy(concat_hash, current_hash, 32);
            memcpy(concat_hash + 32, sibling->hash, 32);
        }
        else if (sibling)
        {
            memcpy(concat_hash, sibling->hash, 32);
            memcpy(concat_hash + 32, current_hash, 32);
        }
        else
        {
            memcpy(concat_hash, current_hash, 32); // No sibling, single child case
        }

        compute_hash(concat_hash, current_hash);
        current = current->parent;
    }

    return compare_hashes(current_hash, expected_root_hash);
}

int get_number_of_blocks(const char *volume_path)
{
    return DATA_BLOCKS_PER_VOLUME;
}

// this function will have to decrypt the block and then compute the hash
void get_block_hash(const char *volume_path, int block_index, char *hash)
{
    char volume_filename[256];
    snprintf(volume_filename, sizeof(volume_filename), "%s/volume.bin", volume_path); // Construct the full volume file path

    FILE *file = fopen(volume_filename, "rb");
    if (file)
    {
        unsigned char block_data[BLOCK_SIZE]; // Buffer to hold block data

        // Move the file pointer to the correct block position
        fseek(file, block_index * BLOCK_SIZE, SEEK_SET);

        // Read the block data from file
        if (fread(block_data, 1, BLOCK_SIZE, file) == BLOCK_SIZE)
        {
            unsigned char temp_hash[SHA256_DIGEST_LENGTH];
            SHA256(block_data, BLOCK_SIZE, temp_hash);     // Compute SHA-256 hash
            memcpy(hash, temp_hash, SHA256_DIGEST_LENGTH); // Copy the computed hash into the provided buffer
        }
        else
        {
            fprintf(stderr, "Error reading block data from volume.\n");
            memset(hash, 0, SHA256_DIGEST_LENGTH); // Clear the hash on read failure
        }

        fclose(file);
    }
    else
    {
        fprintf(stderr, "Failed to open volume file: %s\n", volume_filename);
        memset(hash, 0, SHA256_DIGEST_LENGTH); // Clear the hash if file cannot be opened
    }
}

void generate_random_hash(char *hash, size_t size)
{
    const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < size; i++)
    {
        hash[i] = hex_chars[rand() % 16];
    }
}

MerkleTree *initialize_merkle_tree_for_volume(const char *volume_path)
{
    int num_blocks = get_number_of_blocks(volume_path);
    char **block_hashes = malloc(num_blocks * sizeof(char *));
    for (int i = 0; i < num_blocks; i++)
    {
        block_hashes[i] = malloc(32);              // Assuming SHA-256 hash size
        generate_random_hash(block_hashes[i], 32); // Fill with random data
    }

    MerkleTree *tree = build_merkle_tree(block_hashes, num_blocks);

    // Clean up hashes array
    for (int i = 0; i < num_blocks; i++)
    {
        free(block_hashes[i]);
    }
    free(block_hashes);

    return tree;
}

void save_node(MerkleNode *node, FILE *file)
{
    if (!node)
        return;

    // Write the hash
    fwrite(node->hash, 1, 32, file);
    // Write the block index
    fwrite(&(node->block_index), sizeof(int), 1, file);

    // Indicators for the presence of left and right children
    char has_left = (node->left != NULL);
    char has_right = (node->right != NULL);
    fwrite(&has_left, 1, 1, file);
    fwrite(&has_right, 1, 1, file);

    // Recursively save the left and right children
    save_node(node->left, file);
    save_node(node->right, file);
}

void save_merkle_tree_to_file(MerkleTree *tree, const char *file_path)
{
    FILE *file = fopen(file_path, "wb");
    if (!file)
    {
        fprintf(stderr, "Failed to open file for writing: %s\n", file_path);
        return;
    }

    save_node(tree->root, file);
    fclose(file);
}

MerkleNode *load_node(FILE *file)
{
    char hash[32];
    if (fread(hash, 1, 32, file) != 32)
        return NULL;

    int block_index;
    fread(&block_index, sizeof(int), 1, file);

    char has_left, has_right;
    fread(&has_left, 1, 1, file);
    fread(&has_right, 1, 1, file);

    MerkleNode *node = create_merkle_node(hash, NULL, NULL, block_index);
    if (has_left)
        node->left = load_node(file);
    if (has_right)
        node->right = load_node(file);

    if (node->left)
        node->left->parent = node;
    if (node->right)
        node->right->parent = node;

    return node;
}

MerkleTree *load_merkle_tree_from_file(const char *file_path)
{
    FILE *file = fopen(file_path, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open file for reading: %s\n", file_path);
        return NULL;
    }

    MerkleTree *tree = malloc(sizeof(MerkleTree));

    if (!tree)
    {
        fclose(file);
        return NULL;
    }

    tree->root = load_node(file);
    fclose(file);

    return tree;
}

// Simulated function to retrieve the Merkle tree associated with a volume ID
MerkleTree *get_merkle_tree_for_volume(const char *volume_id)
{
    // This should return the MerkleTree pointer for the specified volume
    return NULL; // Placeholder
}

MerkleNode *find_leaf_node(MerkleTree *tree, int block_index)
{
    // Simple search for the correct leaf node based on block index
    // Implement this based on your tree traversal logic
    return NULL; // Placeholder
}

void update_merkle_node_for_block(const char *volume_id, int block_index, const void *block_data)
{
    MerkleTree *tree = get_merkle_tree_for_volume(volume_id);
    MerkleNode *leaf_node = find_leaf_node(tree, block_index);

    if (leaf_node)
    {
        char new_hash[32];
        compute_hash(block_data, new_hash);
        update_merkle_node(leaf_node, new_hash);
    }
}

// Simulated function to retrieve the current root hash of a Merkle tree for a volume
void get_root_hash(const char *volume_id, char *root_hash)
{
    // Placeholder: Retrieve the root hash stored in system memory or configuration
    strcpy(root_hash, "known_good_root_hash"); // Example placeholder
}

bool verify_block_integrity(const char *volume_id, int block_index)
{
    MerkleTree *tree = get_merkle_tree_for_volume(volume_id);
    MerkleNode *leaf_node = find_leaf_node(tree, block_index);
    char expected_root_hash[32];
    get_root_hash(volume_id, expected_root_hash);

    // read the hash from the block
    char decrypted_hash[32];

    get_block_hash(volume_id, block_index, decrypted_hash);

    if (leaf_node)
    {
        return verify_merkle_path(leaf_node, expected_root_hash, decrypted_hash);
    }
    return false;
}