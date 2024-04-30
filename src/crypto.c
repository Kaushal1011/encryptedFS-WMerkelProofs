// Remove the incompatible language mode flag
#include "crypto.h"
#include <string.h>
unsigned char key[crypto_aead_aes256gcm_KEYBYTES];

// Generate a random nonce
void generate_nonce(unsigned char *nonce)
{
    randombytes_buf(nonce, crypto_aead_aes256gcm_NPUBBYTES);
}

// Generate a random key
void generate_key(unsigned char *key)
{
    crypto_aead_aes256gcm_keygen(key);
}

// Encrypt data using AES-GCM
int encrypt_aes_gcm(unsigned char *ciphertext, unsigned long long *ciphertext_len,
                    const unsigned char *plaintext, unsigned long long plaintext_len,
                    const unsigned char *nonce, const unsigned char *key)
{
    if (crypto_aead_aes256gcm_is_available() == 0)
    {
        return -1; // AES256-GCM not available on this CPU
    }

    return crypto_aead_aes256gcm_encrypt(ciphertext, ciphertext_len,
                                         plaintext, plaintext_len,
                                         NULL, 0, NULL, nonce, key);
}

// Decrypt data using AES-GCM
int decrypt_aes_gcm(unsigned char *decrypted, unsigned long long *decrypted_len,
                    const unsigned char *ciphertext, unsigned long long ciphertext_len,
                    const unsigned char *nonce, const unsigned char *key)
{
    if (crypto_aead_aes256gcm_is_available() == 0)
    {
        return -1; // AES256-GCM not available on this CPU
    }

    if (crypto_aead_aes256gcm_decrypt(decrypted, decrypted_len, NULL,
                                      ciphertext, ciphertext_len, NULL, 0,
                                      nonce, key) != 0)
    {
        return -1; // Decryption failed or data tampered
    }

    return 0;
}

// Function to generate and store the key with filename specified
void generate_and_store_key(const char *filename)
{
    extern unsigned char key[crypto_aead_aes256gcm_KEYBYTES];
    // Generate a random key
    crypto_aead_aes256gcm_keygen(key);

    // Buffer to hold the Base64 encoded key
    char b64_key[sodium_base64_ENCODED_LEN(crypto_aead_aes256gcm_KEYBYTES, sodium_base64_VARIANT_ORIGINAL)];
    // Encode the key in Base64
    sodium_bin2base64(b64_key, sizeof(b64_key), key, sizeof(key), sodium_base64_VARIANT_ORIGINAL);

    // Store the Base64-encoded key in a specified file
    FILE *key_file = fopen(filename, "w+");
    if (key_file == NULL)
    {
        perror("Failed to open file");
        return;
    }
    fprintf(key_file, "%s\n", b64_key);
    fclose(key_file);

    printf("Key stored in %s\n", filename);
}

// Function to load and decode the key from a specified filename
int load_key(unsigned char *key, const char *filename)
{

    printf("Loading key from %s\n", filename);
    char b64_key[sodium_base64_ENCODED_LEN(crypto_aead_aes256gcm_KEYBYTES, sodium_base64_VARIANT_ORIGINAL)];
    FILE *key_file = fopen(filename, "r");
    if (key_file == NULL)
    {
        perror("Failed to open file");
        return -1;
    }

    if (fgets(b64_key, sizeof(b64_key), key_file) == NULL)
    {
        perror("Failed to read key");
        fclose(key_file);
        return -1;
    }
    fclose(key_file);

    // Decode the Base64 string back to binary
    if (sodium_base642bin(key, crypto_aead_aes256gcm_KEYBYTES, b64_key, strlen(b64_key),
                          NULL, NULL, NULL, sodium_base64_VARIANT_ORIGINAL) != 0)
    {
        fprintf(stderr, "Failed to decode Base64 key\n");
        return -1;
    }

    return 0;
}

// Encryption using libsodium (ChaCha20-Poly1305)
int encrypt_data(unsigned char *ciphertext, unsigned long long *ciphertext_len,
                 const unsigned char *plaintext, unsigned long long plaintext_len,
                 const unsigned char *nonce, const unsigned char *key)
{
    if (crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext, ciphertext_len,
                                                  plaintext, plaintext_len,
                                                  NULL, 0, NULL, nonce, key) != 0)
    {
        // Encryption failed
        return -1;
    }
    return 0;
}

// Decryption using libsodium (ChaCha20-Poly1305)
int decrypt_data(unsigned char *decrypted, unsigned long long *decrypted_len,
                 const unsigned char *ciphertext, unsigned long long ciphertext_len,
                 const unsigned char *nonce, const unsigned char *key)
{
    if (crypto_aead_chacha20poly1305_ietf_decrypt(decrypted, decrypted_len,
                                                  NULL,
                                                  ciphertext, ciphertext_len,
                                                  NULL, 0, nonce, key) != 0)
    {
        // Decryption failed
        return -1;
    }
    return 0;
}
