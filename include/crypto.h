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

#endif // CRYPTO_H
