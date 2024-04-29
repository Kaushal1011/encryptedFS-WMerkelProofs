#ifndef CRYPTO_H
#define CRYPTO_H

#include <sodium.h>

// Function declarations
int encrypt_aes_gcm(unsigned char *ciphertext, unsigned long long *ciphertext_len,
                    const unsigned char *plaintext, unsigned long long plaintext_len,
                    const unsigned char *nonce, const unsigned char *key);

int decrypt_aes_gcm(unsigned char *decrypted, unsigned long long *decrypted_len,
                    const unsigned char *ciphertext, unsigned long long ciphertext_len,
                    const unsigned char *nonce, const unsigned char *key);

void generate_nonce(unsigned char *nonce);
void generate_key(unsigned char *key);
void generate_and_store_key(const char *filename);
int load_key(unsigned char *key, const char *filename);
int encrypt_data(unsigned char *ciphertext, unsigned long long *ciphertext_len,
                 const unsigned char *plaintext, unsigned long long plaintext_len,
                 const unsigned char *nonce, const unsigned char *key);
int decrypt_data(unsigned char *decrypted, unsigned long long *decrypted_len,
                 const unsigned char *ciphertext, unsigned long long ciphertext_len,
                 const unsigned char *nonce, const unsigned char *key);

extern unsigned char key[crypto_aead_aes256gcm_KEYBYTES];

#endif // CRYPTO_H
