#ifndef SECURITY_H
#define SECURITY_H

#include <time.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define SALT_LENGTH 16
#define HASH_LENGTH SHA256_DIGEST_LENGTH
#define MAX_PASSWORD_LENGTH 64
#define TOKEN_LENGTH 32

// Rate limiting structure
typedef struct {
    char ip[16];
    time_t last_reset;
    int message_count;
    int connection_count;
} rate_limit_t;

// Authentication structure
typedef struct {
    char username[32];
    unsigned char salt[SALT_LENGTH];
    unsigned char hash[HASH_LENGTH];
    char token[TOKEN_LENGTH + 1];
    time_t token_expiry;
} auth_data_t;

// Function prototypes
static int generate_salt(unsigned char *salt);
static int hash_password(const char *password, const unsigned char *salt, unsigned char *hash);
static int generate_token(char *token);
static bool validate_token(const auth_data_t *auth);
static void reset_rate_limits(rate_limit_t *limits);
static int check_rate_limit(rate_limit_t *limits, int type);

// Generate random salt
static int generate_salt(unsigned char *salt) {
    if (!RAND_bytes(salt, SALT_LENGTH)) {
        return -1;
    }
    return 0;
}

// Hash password with salt
static int hash_password(const char *password, const unsigned char *salt, unsigned char *hash) {
    EVP_MD_CTX *mdctx;
    unsigned int hash_len;
    
    mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return -1;
    }
    
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    if (EVP_DigestUpdate(mdctx, password, strlen(password)) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    if (EVP_DigestUpdate(mdctx, salt, SALT_LENGTH) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    EVP_MD_CTX_free(mdctx);
    return 0;
}

// Generate random token
static int generate_token(char *token) {
    unsigned char random[TOKEN_LENGTH / 2];
    static const char hex[] = "0123456789abcdef";
    
    if (!RAND_bytes(random, sizeof(random))) {
        return -1;
    }
    
    for (size_t i = 0; i < sizeof(random); i++) {
        token[i * 2] = hex[random[i] >> 4];
        token[i * 2 + 1] = hex[random[i] & 0x0F];
    }
    token[TOKEN_LENGTH] = '\0';
    
    return 0;
}

// Validate token
static bool validate_token(const auth_data_t *auth) {
    if (!auth) {
        return false;
    }
    
    time_t now = time(NULL);
    return (now < auth->token_expiry);
}

// Reset rate limits
static void reset_rate_limits(rate_limit_t *limits) {
    if (!limits) {
        return;
    }
    
    time_t now = time(NULL);
    
    // Reset counters if a minute has passed
    if (now - limits->last_reset >= 60) {
        limits->message_count = 0;
        limits->connection_count = 0;
        limits->last_reset = now;
    }
}

// Check rate limit
// type: 0 for messages, 1 for connections
static int check_rate_limit(rate_limit_t *limits, int type) {
    if (!limits) {
        return -1;
    }
    
    reset_rate_limits(limits);
    
    if (type == 0) {
        if (limits->message_count >= RATE_LIMIT_MESSAGES) {
            return -1;
        }
        limits->message_count++;
    } else {
        if (limits->connection_count >= RATE_LIMIT_CONNECTIONS) {
            return -1;
        }
        limits->connection_count++;
    }
    
    return 0;
}

#endif // SECURITY_H 