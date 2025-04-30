#ifndef SECURITY_H
#define SECURITY_H

#include <stdbool.h>
#include <time.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>

// Authentication data structure
typedef struct {
    char username[32];
    char salt[65];
    char token[65];
    time_t token_expiry;
} auth_data_t;

// SSL context types
#define SSL_SERVER 1
#define SSL_CLIENT 2

// Function declarations
bool init_ssl(void);
void cleanup_ssl(void);
bool generate_salt(char *salt);
bool generate_token(char *token);
bool verify_token(const char *token);
bool calculate_file_checksum(const char *filepath, char *checksum);
bool verify_file_checksum(const char *filepath, const char *checksum);
bool encrypt_message(const char *message, const char *key, char *encrypted);
bool decrypt_message(const char *encrypted, const char *key, char *decrypted);
bool generate_keypair(char *public_key, char *private_key);
bool sign_message(const char *message, const char *private_key, char *signature);
bool verify_signature(const char *message, const char *signature, const char *public_key);
SSL_CTX *create_ssl_context(int type);
bool load_server_certificates(SSL_CTX *ctx, const char *cert_file, const char *key_file);

#endif // SECURITY_H 