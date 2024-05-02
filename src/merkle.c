// File: merkle.c
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <math.h>

#include "merkle.h"
#include "volume.h"
#include "constants.h"

void hash_to_hex(const unsigned char *bin, char *hex, size_t len)
{
    const char *hex_digits = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i)
    {
        hex[2 * i] = hex_digits[(bin[i] >> 4) & 0x0F];
        hex[2 * i + 1] = hex_digits[bin[i] & 0x0F];
    }
    hex[2 * len] = '\0'; // null-terminate the string
}

MerkleNode *create_merkle_node(const char *hash, MerkleNode *left, MerkleNode *right, int block_index)
{
    MerkleNode *node = malloc(sizeof(MerkleNode));
    if (node)
    {
        strcpy(node->hash, hash);
        node->left = left;
        node->right = right;
        node->parent = NULL;
        node->block_index = block_index;
        // Set min and max indices
        node->min_index = (left ? left->min_index : block_index);
        node->max_index = (right ? right->max_index : block_index);
    }
    return node;
}

void compute_hash(const char *input, char *output)
{
    unsigned char temp_hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, strlen(input), temp_hash);
    hash_to_hex(temp_hash, output, SHA256_DIGEST_LENGTH); // Convert binary hash to hex string
}

bool compare_hashes(const char *hash1, const char *hash2)
{
    return strcmp(hash1, hash2) == 0; // Use strcmp for string comparison
}

void update_merkle_node(MerkleNode *node, const char *new_hash)
{
    printf("merkle: Updating merkle node\n");
    memcpy(node->hash, new_hash, 65);
    printf("merkle: updated Node hash: %s\n", node->hash);
    // Update the parent nodes
    while (node->parent)
    {
        char concat_hash[130];
        if (node->parent->left && node->parent->right)
        {
            memcpy(concat_hash, node->parent->left->hash, 65);
            memcpy(concat_hash + 65, node->parent->right->hash, 65);
        }
        else
        {
            memcpy(concat_hash, node->hash, 65); // Single child scenario
            memcpy(concat_hash + 65, node->hash, 65);
        }
        compute_hash(concat_hash, node->parent->hash);
        node = node->parent;
    }
}

MerkleTree *build_merkle_tree(char **block_hashes, int num_blocks)
{
    printf("merkle: Building merkle tree\n");

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
            char combined_hash[130];
            memcpy(combined_hash, current_level[2 * i]->hash, 65);
            memcpy(combined_hash + 65, current_level[2 * i + 1]->hash, 65);
            char new_hash[65];
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

bool verify_merkle_path(MerkleNode *leaf_node, const char *expected_root_hash, char decrypted_hash[65])
{
    printf("merkle: Verifying merkle path\n");

    MerkleNode *current = leaf_node;
    char current_hash[65] = {0};
    memcpy(current_hash, decrypted_hash, 65);

    while (current->parent)
    {
        MerkleNode *sibling = (current == current->parent->left) ? current->parent->right : current->parent->left;
        char concat_hash[130];
        if (current == current->parent->left && sibling)
        {
            memcpy(concat_hash, current_hash, 65);
            memcpy(concat_hash + 65, sibling->hash, 65);
        }
        else if (sibling)
        {
            memcpy(concat_hash, sibling->hash, 65);
            memcpy(concat_hash + 65, current_hash, 65);
        }
        else
        {
            memcpy(concat_hash, current_hash, 65); // No sibling, single child case
        }

        compute_hash(concat_hash, current_hash);
        current = current->parent;
    }

    printf("merkle: Computed root hash: %s\n", current_hash);
    printf("merkle: Expected root hash: %s\n", expected_root_hash);

    if (compare_hashes(current_hash, expected_root_hash))
    {
        printf("merkle: Root hash matches -> Verified\n");
        return true;
    }
    else
    {
        printf("merkle: Root hash does not match -> Not Verified\n");
    }

    return compare_hashes(current_hash, expected_root_hash);
}

int get_number_of_blocks(char *volume_path)
{
    return DATA_BLOCKS_PER_VOLUME;
}

// this function will have to decrypt the block and then compute the hash
void get_block_hash(int block_index, char *hash)
{
    printf("merkle: Getting block hash\n");

    char block_data[BLOCK_SIZE];
    read_volume_block_no_check(block_index, block_data);
    compute_hash(block_data, hash);
    printf("merkle: Block Hash: %s\n", hash);
}

void generate_random_hash(char *hash, size_t size)
{
    const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < size; i++)
    {
        hash[i] = hex_chars[rand() % 15];
    }
}

MerkleTree *initialize_merkle_tree_for_volume(char *volume_path)
{
    printf("merkle: Initializing merkle tree\n");

    int num_blocks = get_number_of_blocks(volume_path);
    char **block_hashes = malloc(num_blocks * sizeof(char *));
    for (int i = 0; i < num_blocks; i++)
    {
        block_hashes[i] = malloc(65);              // Assuming SHA-256 hash size
        generate_random_hash(block_hashes[i], 65); // Fill with random data
        block_hashes[i][65] = '\0';                // Null-terminate the string
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
    {
        fprintf(file, "null\n"); // Marker for no node
        return;
    }
    // Save current node's data
    fprintf(file, "%s %d\n", node->hash, node->block_index);

    // Recursively save left child
    if (node->left)
    {
        fprintf(file, "left\n"); // Indicate the start of a left child
        save_node(node->left, file);
    }
    else
    {
        fprintf(file, "null\n"); // No left child
    }

    // Recursively save right child
    if (node->right)
    {
        fprintf(file, "right\n"); // Indicate the start of a right child
        save_node(node->right, file);
    }
    else
    {
        fprintf(file, "null\n"); // No right child
    }
}

void save_merkle_tree_to_file(MerkleTree *tree, const char *file_path)
{
    printf("merkle: Saving merkle tree to file\n");

    printf("merkle: File path: %s\n", file_path);

    FILE *file = fopen(file_path, "wb");
    if (!file)
    {
        fprintf(stderr, "Failed to open file for writing: %s\n", file_path);
        return;
    }

    save_node(tree->root, file);
    fclose(file);

    printf("Merkle tree saved to file\n");
}

MerkleNode *load_node(FILE *file)
{
    char line[128];
    if (!fgets(line, sizeof(line), file) || strcmp(line, "null\n") == 0)
    {
        return NULL; // No node to load
    }

    char hash[65];
    int block_index;
    if (sscanf(line, "%65s %d", hash, &block_index) != 2)
    {
        fprintf(stderr, "Failed to read node data.\n");
        return NULL;
    }

    MerkleNode *node = create_merkle_node(hash, NULL, NULL, block_index);

    // Load left child if present
    if (fgets(line, sizeof(line), file) && strcmp(line, "left\n") == 0)
    {
        node->left = load_node(file);
        if (node->left)
        {
            node->left->parent = node;
        }
    }

    // Load right child if present
    if (fgets(line, sizeof(line), file) && strcmp(line, "right\n") == 0)
    {
        node->right = load_node(file);
        if (node->right)
        {
            node->right->parent = node;
        }
    }

    // Update min and max indices
    if (node->left && node->right)
    {
        node->min_index = node->left->min_index;
        node->max_index = node->right->max_index;
    }
    else if (node->left)
    {
        node->min_index = node->left->min_index;
        node->max_index = node->left->max_index;
    }
    else if (node->right)
    {
        node->min_index = node->right->min_index;
        node->max_index = node->right->max_index;
    }
    else
    {
        node->min_index = node->max_index = block_index;
    }

    return node;
}

MerkleTree *load_merkle_tree_from_file(const char *file_path)
{
    printf("merkle: Loading merkle tree from file\n");
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

MerkleTree *get_merkle_tree_for_volume(char *volume_id)
{
    printf("merkle: Getting merkle tree for volume %s\n", volume_id);
    extern superblock_t sb;
    printf("merkle: Volume tree: %p\n", sb.volumes[atoi(volume_id)].merkle_tree);
    // volume id is the index of the volume in the superblock
    return sb.volumes[atoi(volume_id)].merkle_tree;
}

MerkleNode *find_leaf_node(MerkleNode *node, int block_index)
{
    printf("merkle: Finding leaf node\n");

    printf("merkle: Node: %p\n", node);

    printf("merkle: Block index: %d\n", block_index);

    printf("merkle: Node block index: %d %d %d \n", node->block_index, node->min_index, node->max_index);

    if (!node || (block_index < node->min_index || block_index > node->max_index))
    {
        printf("Returning NULL\n");
        return NULL;
    }

    // Direct hit for leaf nodes
    if (node->block_index == block_index)
    {
        printf("merkle: Returning node\n");
        return node;
    }

    // Decide which subtree to search based on the range
    MerkleNode *result = find_leaf_node(node->left, block_index);
    if (result == NULL)
    {
        printf("merkle: Searching right\n");
        result = find_leaf_node(node->right, block_index);
    }
    printf("merkle: Returning result\n");
    return result;
}

// Wrapper function to start the search from the tree root
MerkleNode *find_leaf_node_in_tree(MerkleTree *tree, int block_index)
{
    printf("merkle: Finding leaf node in tree\n");

    if (!tree)
    {
        return NULL;
    }

    MerkleNode *leaf = find_leaf_node(tree->root, block_index);
    if (leaf)
    {
        printf("merkle: Leaf node found: %s\n", leaf->hash);
        return leaf;
    }
    return NULL;
}

void update_merkle_node_for_block(char *volume_id, int block_index, const void *block_data)
{
    extern superblock_t sb;
    printf("merkle: Updating merkle node for block\n");

    MerkleTree *tree = get_merkle_tree_for_volume(volume_id);
    MerkleNode *leaf_node = find_leaf_node_in_tree(tree, block_index);

    printf("merkle: Leaf node found: %s\n", leaf_node->hash);
    printf("merkle: Lead node block index %d\n", leaf_node->block_index);

    if (leaf_node)
    {
        char new_hash[65];
        compute_hash(block_data, new_hash);
        printf("merkle: New hash: %s\n", new_hash);
        update_merkle_node(leaf_node, new_hash);
        bool res = compare_hashes(leaf_node->hash, new_hash);
        printf("merkle: Hash comparison result: %d\n", res);
        if (res)
        {
            printf("merkle: Hashes match (stored and computed for leaf)\n");
        }
        else
        {
            printf("merkle: Hashes do not match (stored and computed for leaf)\n");
        }
    }

    printf("merkle: Saving merkle tree to file\n");
    // save the updated tree to file
    save_merkle_tree_to_file(tree, sb.volumes[atoi(volume_id)].merkle_path);
}

void get_root_hash(char *volume_id, char *root_hash)
{
    MerkleTree *tree = get_merkle_tree_for_volume(volume_id);
    if (tree)
    {
        memcpy(root_hash, tree->root->hash, 65);
    }
    else
    {
        memset(root_hash, 0, 65);
    }
}

//  take decrypted block hash as functiton param
bool verify_block_integrity(int block_index)
{
    char volume_id[9] = "0";
    int volume_id_int = block_index / DATA_BLOCKS_PER_VOLUME;
    sprintf(volume_id, "%d", volume_id_int);

    int block_index_in_volume = block_index % DATA_BLOCKS_PER_VOLUME;

    printf("merkle: Verifying block integrity\n");
    MerkleTree *tree = get_merkle_tree_for_volume(volume_id);
    MerkleNode *leaf_node = find_leaf_node_in_tree(tree, block_index_in_volume);
    printf("merkle: Leaf node found: %s\n", leaf_node->hash);
    printf("merkle: Leaf node block index %d\n", leaf_node->block_index);
    char expected_root_hash[65];
    get_root_hash(volume_id, expected_root_hash);

    printf("merkle: Expected root hash: %s\n", expected_root_hash);

    // read the hash from the block
    char decrypted_hash[65];

    get_block_hash(block_index, decrypted_hash);

    printf("merkle: Decrypted hash: %s\n", decrypted_hash);
    printf("merkle: Leaf node: %s\n", leaf_node->hash);

    if (leaf_node)
    {
        return verify_merkle_path(leaf_node, expected_root_hash, decrypted_hash);
    }
    return false;
}