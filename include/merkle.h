#ifndef MERKLE_H
#define MERKLE_H

#include <stdbool.h>
#include <stdio.h>

// Define constants such as BLOCK_SIZE if not already defined
#ifndef BLOCK_SIZE
#define BLOCK_SIZE 4096 // Default block size
#endif

// Define the SHA256 digest length if not defined
#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH 32
#endif

// Merkle tree node structure
typedef struct MerkleNode
{
    int block_index;           // Index of the block in the volume for searching purposes
    int min_index;             // Minimum index in the subtree
    int max_index;             // Maximum index in the subtree
    char hash[65];             // Hash stored in this node
    struct MerkleNode *left;   // Pointer to left child
    struct MerkleNode *right;  // Pointer to right child
    struct MerkleNode *parent; // Pointer to parent node
} MerkleNode;

// Merkle tree structure
typedef struct
{
    MerkleNode *root; // Root node of the Merkle tree
    int depth;        // Depth of the tree (optional)
} MerkleTree;

// Function prototypes for managing Merkle trees
MerkleNode *create_merkle_node(const char *hash, MerkleNode *left, MerkleNode *right, int block_index);
void compute_hash(const char *input, char *output);
bool compare_hashes(const char *hash1, const char *hash2);
void update_merkle_node(MerkleNode *node, const char *new_hash);
MerkleTree *build_merkle_tree(char **block_hashes, int num_blocks);
bool verify_merkle_path(MerkleNode *leaf_node, const char *expected_root_hash, char decrytped_hash[65]);
void save_merkle_tree_to_file(MerkleTree *tree, const char *file_path);
MerkleTree *load_merkle_tree_from_file(const char *file_path);

// Block management related functions
int get_number_of_blocks(char *volume_path);
void get_block_hash(int block_index, char *hash);
void generate_random_hash(char *hash, size_t size);

// Merkle tree volume operations
MerkleTree *initialize_merkle_tree_for_volume(char *volume_path);
MerkleTree *get_merkle_tree_for_volume(char *volume_id);
MerkleNode *find_leaf_node(MerkleNode *node, int block_index);
MerkleNode *find_leaf_node_in_tree(MerkleTree *tree, int block_index);
void update_merkle_node_for_block(char *volume_id, int block_index, const void *block_data);
void get_root_hash(char *volume_id, char *root_hash);
bool verify_block_integrity(int block_index);

#endif // MERKLE_H
