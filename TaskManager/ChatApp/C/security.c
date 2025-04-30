#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "security.h"

#define SALT_LENGTH 32
#define TOKEN_LENGTH 32

// Initialize OpenSSL
bool init_ssl(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    return true;
}

// Cleanup OpenSSL
void cleanup_ssl(void) {
    EVP_cleanup();
    ERR_free_strings();
}

// Generate random salt
bool generate_salt(char *salt) {
    unsigned char raw_salt[SALT_LENGTH];
    if (RAND_bytes(raw_salt, SALT_LENGTH) != 1) {
        return false;
    }
    
    for (int i = 0; i < SALT_LENGTH; i++) {
        sprintf(salt + (i * 2), "%02x", raw_salt[i]);
    }
    salt[SALT_LENGTH * 2] = '\0';
    return true;
}

// Generate authentication token
bool generate_token(char *token) {
    unsigned char raw_token[TOKEN_LENGTH];
    if (RAND_bytes(raw_token, TOKEN_LENGTH) != 1) {
        return false;
    }
    
    for (int i = 0; i < TOKEN_LENGTH; i++) {
        sprintf(token + (i * 2), "%02x", raw_token[i]);
    }
    token[TOKEN_LENGTH * 2] = '\0';
    return true;
}

// Verify authentication token
bool verify_token(const char *token) {
    // Simple length check for now
    return strlen(token) == TOKEN_LENGTH * 2;
}

// Calculate SHA-256 checksum of file using EVP
bool calculate_file_checksum(const char *filepath, char *checksum) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return false;
    
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        fclose(file);
        return false;
    }
    
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return false;
    }
    
    unsigned char buffer[4096];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes) != 1) {
            EVP_MD_CTX_free(mdctx);
            fclose(file);
            return false;
        }
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return false;
    }
    
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(checksum + (i * 2), "%02x", hash[i]);
    }
    checksum[hash_len * 2] = '\0';
    
    EVP_MD_CTX_free(mdctx);
    fclose(file);
    return true;
}

// Verify file checksum
bool verify_file_checksum(const char *filepath, const char *checksum) {
    char calculated[EVP_MAX_MD_SIZE * 2 + 1];
    if (!calculate_file_checksum(filepath, calculated)) {
        return false;
    }
    return strcmp(calculated, checksum) == 0;
}

// Encrypt message using AES-256-GCM
bool encrypt_message(const char *message, const char *key, char *encrypted) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    
    unsigned char iv[12];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (unsigned char *)key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    int len, encrypted_len = 0;
    unsigned char *encrypted_text = malloc(strlen(message) + EVP_MAX_BLOCK_LENGTH);
    
    if (EVP_EncryptUpdate(ctx, encrypted_text, &len, (unsigned char *)message, strlen(message)) != 1) {
        free(encrypted_text);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    encrypted_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, encrypted_text + len, &len) != 1) {
        free(encrypted_text);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    encrypted_len += len;
    
    unsigned char tag[16];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
        free(encrypted_text);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    // Format: IV (12 bytes) || Tag (16 bytes) || Ciphertext
    memcpy(encrypted, iv, 12);
    memcpy(encrypted + 12, tag, 16);
    memcpy(encrypted + 28, encrypted_text, encrypted_len);
    
    free(encrypted_text);
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

// Decrypt message using AES-256-GCM
bool decrypt_message(const char *encrypted, const char *key, char *decrypted) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    
    unsigned char iv[12];
    unsigned char tag[16];
    memcpy(iv, encrypted, 12);
    memcpy(tag, encrypted + 12, 16);
    const unsigned char *ciphertext = (unsigned char *)encrypted + 28;
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (unsigned char *)key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    int len, plaintext_len = 0;
    if (EVP_DecryptUpdate(ctx, (unsigned char *)decrypted, &len, ciphertext, strlen(encrypted) - 28) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintext_len = len;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    
    if (EVP_DecryptFinal_ex(ctx, (unsigned char *)decrypted + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    plaintext_len += len;
    decrypted[plaintext_len] = '\0';
    
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

// Generate RSA key pair
bool generate_keypair(char *public_key, char *private_key) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) return false;
    
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }
    
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }
    
    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }
    
    BIO *pub = BIO_new(BIO_s_mem());
    BIO *priv = BIO_new(BIO_s_mem());
    
    PEM_write_bio_PUBKEY(pub, pkey);
    PEM_write_bio_PrivateKey(priv, pkey, NULL, NULL, 0, NULL, NULL);
    
    BIO_read(pub, public_key, 8192);
    BIO_read(priv, private_key, 8192);
    
    EVP_PKEY_free(pkey);
    BIO_free(pub);
    BIO_free(priv);
    EVP_PKEY_CTX_free(ctx);
    return true;
}

// Sign message using RSA private key
bool sign_message(const char *message, const char *private_key, char *signature) {
    BIO *bio = BIO_new_mem_buf(private_key, -1);
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);
    
    if (!pkey) return false;
    
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return false;
    }
    
    if (EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, pkey) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return false;
    }
    
    if (EVP_DigestSignUpdate(ctx, message, strlen(message)) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return false;
    }
    
    size_t sig_len;
    if (EVP_DigestSignFinal(ctx, NULL, &sig_len) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return false;
    }
    
    unsigned char *sig = malloc(sig_len);
    if (EVP_DigestSignFinal(ctx, sig, &sig_len) <= 0) {
        free(sig);
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return false;
    }
    
    // Convert to hex string
    for (size_t i = 0; i < sig_len; i++) {
        sprintf(signature + (i * 2), "%02x", sig[i]);
    }
    signature[sig_len * 2] = '\0';
    
    free(sig);
    EVP_PKEY_free(pkey);
    EVP_MD_CTX_free(ctx);
    return true;
}

// Verify signature using RSA public key
bool verify_signature(const char *message, const char *signature, const char *public_key) {
    BIO *bio = BIO_new_mem_buf(public_key, -1);
    EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);
    
    if (!pkey) return false;
    
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return false;
    }
    
    if (EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pkey) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return false;
    }
    
    if (EVP_DigestVerifyUpdate(ctx, message, strlen(message)) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return false;
    }
    
    // Convert hex string to binary
    size_t sig_len = strlen(signature) / 2;
    unsigned char *sig = malloc(sig_len);
    for (size_t i = 0; i < sig_len; i++) {
        sscanf(signature + (i * 2), "%2hhx", &sig[i]);
    }
    
    int ret = EVP_DigestVerifyFinal(ctx, sig, sig_len);
    
    free(sig);
    EVP_PKEY_free(pkey);
    EVP_MD_CTX_free(ctx);
    
    return ret == 1;
}

// Create SSL context based on type (server or client)
SSL_CTX *create_ssl_context(int type) {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    if (type == SSL_SERVER) {
        method = TLS_server_method();
    } else {
        method = TLS_client_method();
    }

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    // Set minimum TLS version
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    
    // Set cipher list to secure ciphers
    SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5:!RC4");
    
    // Enable verification
    if (type == SSL_CLIENT) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    }
    
    // Set verification depth
    SSL_CTX_set_verify_depth(ctx, 4);
    
    // Enable automatic retry on interrupted reads/writes
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

    return ctx;
}

// Load server certificates
bool load_server_certificates(SSL_CTX *ctx, const char *cert_file, const char *key_file) {
    if (!ctx || !cert_file || !key_file) {
        return false;
    }

    // Load certificate chain
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Verify private key
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        return false;
    }

    // Load trusted CA certificates
    if (SSL_CTX_load_verify_locations(ctx, "certs/ca.crt", "certs") <= 0) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    return true;
} 