#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "network_config.h"
#include "security.h"
#include "ui_components.h"

// Global variables
static bool server_running = true;
static int server_socket;
static SSL_CTX *ssl_ctx;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
void cleanup(void);
void handle_signal(int sig);
void *handle_client(void *arg);
void cleanup_client(int index);
void broadcast_message(const char *message, int sender_index);
void handle_private_message(int sender_index, const char *recipient, const char *message);
void handle_room_message(int sender_index, int room_id, const char *message);

// Client structure
typedef struct {
    int socket;
    SSL *ssl;
    char username[MAX_USERNAME_LENGTH];
    user_status_t status;
    int current_room;
    pthread_t thread;
    bool authenticated;
} client_t;

static client_t clients[MAX_CLIENTS];
static int client_count = 0;

// Function prototypes
void *handle_client(void *arg);
void cleanup_client(int index);
void broadcast_message(const char *message, int sender_index);
void handle_private_message(int sender_index, const char *recipient, const char *message);
void handle_room_message(int sender_index, int room_id, const char *message);
void signal_handler(int sig);

// Initialize server
void init_server(int port) {
    struct sockaddr_in server_addr;
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure socket
    if (configure_tcp_socket(server_socket) < 0) {
        perror("Socket configuration failed");
        exit(EXIT_FAILURE);
    }
    
    // Initialize address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_socket, 10) < 0) {
        perror("Socket listen failed");
        exit(EXIT_FAILURE);
    }
    
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // Create SSL context
    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        perror("SSL context creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Load certificates
    if (SSL_CTX_use_certificate_file(ssl_ctx, "certs/server.crt", SSL_FILETYPE_PEM) <= 0) {
        perror("Certificate loading failed");
        exit(EXIT_FAILURE);
    }
    
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, "certs/server.key", SSL_FILETYPE_PEM) <= 0) {
        perror("Private key loading failed");
        exit(EXIT_FAILURE);
    }
}

// Cleanup server
void cleanup_server() {
    // Close all client connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0) {
            cleanup_client(i);
        }
    }
    
    // Close server socket
    close(server_socket);
    
    // Cleanup OpenSSL
    SSL_CTX_free(ssl_ctx);
    EVP_cleanup();
    ERR_free_strings();
}

// Handle client connection
void *handle_client(void *arg) {
    int index = (int)(intptr_t)arg;
    char buffer[MAX_MESSAGE_SIZE];
    ssize_t bytes_read;
    
    // Get username
    bytes_read = SSL_read(clients[index].ssl, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        cleanup_client(index);
        return NULL;
    }
    buffer[bytes_read] = '\0';
    
    // Verify username format
    if (strlen(buffer) >= MAX_USERNAME_LENGTH || strchr(buffer, ' ') != NULL) {
        const char *error = "Invalid username format";
        SSL_write(clients[index].ssl, error, strlen(error));
        cleanup_client(index);
        return NULL;
    }
    
    // Store username
    strncpy(clients[index].username, buffer, MAX_USERNAME_LENGTH - 1);
    clients[index].username[MAX_USERNAME_LENGTH - 1] = '\0';
    clients[index].status = STATUS_ONLINE;
    clients[index].authenticated = true;
    
    // Broadcast join message
    char join_msg[MAX_MESSAGE_SIZE];
    snprintf(join_msg, sizeof(join_msg), "%s has joined the chat", clients[index].username);
    broadcast_message(join_msg, index);
    
    // Main message loop
    while (server_running) {
        bytes_read = SSL_read(clients[index].ssl, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            break;
        }
        buffer[bytes_read] = '\0';
        
        // Parse and handle message
        if (strncmp(buffer, "/msg ", 5) == 0) {
            char *recipient = strtok(buffer + 5, " ");
            char *message = strtok(NULL, "");
            if (recipient && message) {
                handle_private_message(index, recipient, message);
            }
        } else if (strncmp(buffer, "/room ", 6) == 0) {
            char *room_str = strtok(buffer + 6, " ");
            char *message = strtok(NULL, "");
            if (room_str && message) {
                int room_id = atoi(room_str);
                handle_room_message(index, room_id, message);
            }
        } else {
            broadcast_message(buffer, index);
        }
    }
    
    // Cleanup and exit
    char leave_msg[MAX_MESSAGE_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "%s has left the chat", clients[index].username);
    broadcast_message(leave_msg, index);
    cleanup_client(index);
    return NULL;
}

// Cleanup client connection
void cleanup_client(int index) {
    pthread_mutex_lock(&clients_mutex);
    if (clients[index].socket != 0) {
        SSL_shutdown(clients[index].ssl);
        SSL_free(clients[index].ssl);
        close(clients[index].socket);
        clients[index].socket = 0;
        clients[index].ssl = NULL;
        clients[index].authenticated = false;
        client_count--;
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Broadcast message to all clients
void broadcast_message(const char *message, int sender_index) {
    char formatted[MAX_MESSAGE_SIZE];
    snprintf(formatted, sizeof(formatted), "%s: %s", 
             clients[sender_index].username, message);
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != sender_index && clients[i].socket != 0 && clients[i].authenticated) {
            SSL_write(clients[i].ssl, formatted, strlen(formatted));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Handle private message
void handle_private_message(int sender_index, const char *recipient, const char *message) {
    char formatted[MAX_MESSAGE_SIZE];
    snprintf(formatted, sizeof(formatted), "[PM from %s] %s", 
             clients[sender_index].username, message);
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0 && clients[i].authenticated &&
            strcmp(clients[i].username, recipient) == 0) {
            SSL_write(clients[i].ssl, formatted, strlen(formatted));
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Handle room message
void handle_room_message(int sender_index, int room_id, const char *message) {
    char formatted[MAX_MESSAGE_SIZE];
    snprintf(formatted, sizeof(formatted), "[Room %d] %s: %s",
             room_id, clients[sender_index].username, message);
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != sender_index && clients[i].socket != 0 && 
            clients[i].authenticated && clients[i].current_room == room_id) {
            SSL_write(clients[i].ssl, formatted, strlen(formatted));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return EXIT_FAILURE;
    }

    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    // Create SSL context
    ssl_ctx = create_ssl_context(SSL_SERVER);
    if (!ssl_ctx) {
        fprintf(stderr, "Failed to create SSL context\n");
        return EXIT_FAILURE;
    }

    // Load server certificate and private key
    if (!load_server_certificates(ssl_ctx, "certs/server.crt", "certs/server.key")) {
        fprintf(stderr, "Failed to load certificates\n");
        SSL_CTX_free(ssl_ctx);
        return EXIT_FAILURE;
    }

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        SSL_CTX_free(ssl_ctx);
        return EXIT_FAILURE;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_socket);
        SSL_CTX_free(ssl_ctx);
        return EXIT_FAILURE;
    }

    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        SSL_CTX_free(ssl_ctx);
        return EXIT_FAILURE;
    }

    // Listen for connections
    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        close(server_socket);
        SSL_CTX_free(ssl_ctx);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d\n", port);

    // Initialize client array
    memset(clients, 0, sizeof(clients));

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Accept client connections
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

        if (client_socket < 0) {
            if (server_running) {
                perror("Accept failed");
            }
            continue;
        }

        // Find free client slot
        int slot = -1;
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].socket) {
                slot = i;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (slot == -1) {
            fprintf(stderr, "Server full\n");
            close(client_socket);
            continue;
        }

        // Set up SSL
        SSL *ssl = SSL_new(ssl_ctx);
        SSL_set_fd(ssl, client_socket);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client_socket);
            continue;
        }

        // Initialize client structure
        pthread_mutex_lock(&clients_mutex);
        clients[slot].socket = client_socket;
        clients[slot].ssl = ssl;
        clients[slot].authenticated = false;
        clients[slot].status = STATUS_ONLINE;
        clients[slot].current_room = 0;

        // Create client thread
        if (pthread_create(&clients[slot].thread, NULL, handle_client, &clients[slot]) != 0) {
            fprintf(stderr, "Failed to create client thread\n");
            SSL_free(ssl);
            close(client_socket);
            memset(&clients[slot], 0, sizeof(client_t));
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    // Cleanup
    cleanup();
    return EXIT_SUCCESS;
}

// Cleanup resources
void cleanup(void) {
    // Close all client connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0) {
            cleanup_client(i);
        }
    }
    
    // Close server socket
    if (server_socket > 0) {
        close(server_socket);
        server_socket = 0;
    }
    
    // Cleanup OpenSSL
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
    
    EVP_cleanup();
    ERR_free_strings();
}

// Signal handler
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    server_running = false;
    
    // Force accept() to return by connecting to the server socket
    struct sockaddr_in addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080); // Use same port as server
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
} 