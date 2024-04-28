#include "crypto.h"

// Generate a random nonce
void generate_nonce(unsigned char *nonce) {
    randombytes_buf(nonce, crypto_aead_aes256gcm_NPUBBYTES);
}

// Generate a random key
void generate_key(unsigned char *key) {
    crypto_aead_aes256gcm_keygen(key);
}

// Encrypt data using AES-GCM
int encrypt_aes_gcm(unsigned char *ciphertext, unsigned long long *ciphertext_len,
                    const unsigned char *plaintext, unsigned long long plaintext_len,
                    const unsigned char *nonce, const unsigned char *key) {
    if (crypto_aead_aes256gcm_is_available() == 0) {
        return -1; // AES256-GCM not available on this CPU
    }

    return crypto_aead_aes256gcm_encrypt(ciphertext, ciphertext_len,
                                         plaintext, plaintext_len,
                                         NULL, 0, NULL, nonce, key);
}

// Decrypt data using AES-GCM
int decrypt_aes_gcm(unsigned char *decrypted, unsigned long long *decrypted_len,
                    const unsigned char *ciphertext, unsigned long long ciphertext_len,
                    const unsigned char *nonce, const unsigned char *key) {
    if (crypto_aead_aes256gcm_is_available() == 0) {
        return -1; // AES256-GCM not available on this CPU
    }

    if (crypto_aead_aes256gcm_decrypt(decrypted, decrypted_len, NULL,
                                      ciphertext, ciphertext_len, NULL, 0,
                                      nonce, key) != 0) {
        return -1; // Decryption failed or data tampered
    }

    return 0;
}