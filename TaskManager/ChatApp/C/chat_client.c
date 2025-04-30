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
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "network_config.h"
#include "security.h"
#include "ui_components.h"

#define BUFFER_SIZE 4096
#define NAME_LENGTH 32
#define MAX_FILE_SIZE (100 * 1024 * 1024) // 100MB
#define MAX_USERS_PER_ROOM 20

// Global variables
SSL *ssl;
SSL_CTX *ssl_ctx;
int client_socket;
bool connected = false;
bool authenticated = false;
char username[NAME_LENGTH];
bool running = true;
int current_room = 0;
auth_data_t auth;
ui_state_t ui_state;

// Function prototypes
void *receive_messages(void *arg);
void handle_signal(int sig);
void connect_to_server(const char *server_ip, int server_port);
void authenticate(const char *username);
void cleanup();
void show_help();
void handle_file_receive(char *message);
void send_file(const char *recipient, const char *filepath);
void join_room(int room_id);
void create_room(const char *room_name);
void set_status(const char *status, const char *message);
void list_users();
void handle_admin_command(const char *command);
void update_ui();

// Initialize OpenSSL
void init_openssl() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
        perror("Failed to create SSL context");
        exit(EXIT_FAILURE);
    }
    
    // Set up certificate verification
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_load_verify_locations(ssl_ctx, "certs/ca.crt", NULL);
}

// Show available commands
void show_help() {
    ui_clear_screen();
    printf("\n=== Chat Client Help ===\n\n");
    printf("Basic Commands:\n");
    printf("  /msg <username> <message> - Send private message\n");
    printf("  /status <online|away|busy> [message] - Change status\n");
    printf("  /join <room_id> - Join a chat room\n");
    printf("  /create <room_name> - Create a new chat room\n");
    printf("  /list - Show online users\n");
    printf("  /rooms - List available rooms\n");
    printf("  /help - Show this help message\n");
    printf("  /exit - Exit the chat\n\n");
    
    printf("File Operations:\n");
    printf("  /file <username> <filepath> - Send file to user\n");
    printf("  /files - List received files\n");
    printf("  /download <file_id> - Download received file\n\n");
    
    printf("Room Commands:\n");
    printf("  /invite <username> - Invite user to current room\n");
    printf("  /kick <username> - Kick user from current room (admin only)\n");
    printf("  /topic <text> - Set room topic (admin only)\n\n");
    
    printf("Admin Commands:\n");
    printf("  /admin status - Show server status\n");
    printf("  /admin users - List all users\n");
    printf("  /admin ban <username> - Ban user\n");
    printf("  /admin unban <username> - Unban user\n");
    printf("  /admin broadcast <message> - Broadcast message\n\n");
    
    printf("Press Enter to continue...");
    getchar();
    update_ui();
}

// Handle received file
void handle_file_receive(char *message) {
    char sender[NAME_LENGTH];
    char filename[256];
    char filesize[32];
    char checksum[65];
    
    // Parse file notification
    if (sscanf(message, "FILE:%[^:]:%[^:]:%[^:]:%s", 
               sender, filename, filesize, checksum) != 4) {
        ui_show_error("Invalid file notification format");
        return;
    }
    
    long long size = atoll(filesize);
    if (size > MAX_FILE_SIZE) {
        ui_show_error("File too large (max size: 100MB)");
        return;
    }
    
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "downloads/%s", filename);
    
    ui_show_file_progress(sender, filename, size);
    
    // Receive file data
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        ui_show_error("Failed to create file");
        return;
    }
    
    char buffer[BUFFER_SIZE];
    size_t total = 0;
    ssize_t bytes;
    
    while (total < (size_t)size && (bytes = SSL_read(ssl, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes, fp);
        total += bytes;
        ui_update_progress(total, size);
    }
    
    fclose(fp);
    
    // Verify checksum
    if (verify_file_checksum(filepath, checksum)) {
        ui_show_success("File received successfully");
    } else {
        ui_show_error("File verification failed");
        unlink(filepath);
    }
}

// Send file to user
void send_file(const char *recipient, const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ui_show_error("File not found");
        return;
    }
    
    if (st.st_size > MAX_FILE_SIZE) {
        ui_show_error("File too large (max size: 100MB)");
        return;
    }
    
    // Calculate checksum
    char checksum[65];
    if (!calculate_file_checksum(filepath, checksum)) {
        ui_show_error("Failed to calculate checksum");
        return;
    }
    
    // Get filename from path
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    
    // Send file command
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "FILE:%s:%s:%lld:%s", 
             recipient, filename, (long long)st.st_size, checksum);
    SSL_write(ssl, command, strlen(command));
    
    // Send file content
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        ui_show_error("Failed to open file");
        return;
    }
    
    ui_show_file_progress("You", filename, st.st_size);
    
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t total = 0;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (SSL_write(ssl, buffer, bytes_read) < 0) {
            ui_show_error("Failed to send file");
            break;
        }
        total += bytes_read;
        ui_update_progress(total, st.st_size);
    }
    
    fclose(fp);
    ui_show_success("File sent successfully");
}

// Connect to chat server
void connect_to_server(const char *server_ip, int server_port) {
    struct sockaddr_in server_addr;
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        ui_show_error("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure TCP socket
    if (configure_tcp_socket(client_socket) < 0) {
        ui_show_error("Failed to configure socket");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        ui_show_error("Invalid address");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    ui_show_status("Connecting to server...");
    
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ui_show_error("Connection failed");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Set up SSL
    ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, client_socket);
    
    if (SSL_connect(ssl) != 1) {
        ui_show_error("SSL connection failed");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Verify certificate
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        ui_show_error("No certificate presented by server");
        cleanup();
        exit(EXIT_FAILURE);
    }
    X509_free(cert);
    
    connected = true;
    ui_show_success("Connected to server");
}

// Authenticate with server
void authenticate(const char *username) {
    // Send username to server
    SSL_write(ssl, username, strlen(username));
    
    // Wait for server response
    char response[BUFFER_SIZE];
    ssize_t bytes = SSL_read(ssl, response, sizeof(response) - 1);
    if (bytes <= 0) {
        ui_show_error("Authentication failed");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    response[bytes] = '\0';
    if (strcmp(response, "Invalid username format") == 0) {
        ui_show_error(response);
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    authenticated = true;
    ui_show_success("Authentication successful");
}

// Cleanup resources
void cleanup() {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }
    
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
    
    if (client_socket > 0) {
        close(client_socket);
        client_socket = 0;
    }
    
    EVP_cleanup();
    ERR_free_strings();
}

// Signal handler
void handle_signal(int sig) {
    printf("\nReceived signal %d, exiting...\n", sig);
    running = false;
}

// Message receiver thread
void *receive_messages(void *arg) {
    (void)arg; // Unused parameter
    char buffer[BUFFER_SIZE];
    ssize_t bytes;
    
    while (running) {
        bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) {
            if (running) {
                ui_show_error("Connection lost");
                running = false;
            }
            break;
        }
        
        buffer[bytes] = '\0';
        
        // Handle different message types
        if (strncmp(buffer, "FILE:", 5) == 0) {
            handle_file_receive(buffer);
        } else {
            ui_update_chat_window(buffer);
        }
    }
    
    return NULL;
}

// Update UI
void update_ui() {
    ui_clear_screen();
    ui_draw_borders();
    ui_refresh();
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Initialize UI
    ui_init();
    
    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Initialize OpenSSL
    init_openssl();
    
    // Connect to server
    connect_to_server(argv[1], atoi(argv[2]));
    
    // Get username
    ui_get_username(username, sizeof(username));
    
    // Authenticate
    authenticate(username);
    
    // Start message receiver thread
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        ui_show_error("Failed to create receiver thread");
        cleanup();
        return EXIT_FAILURE;
    }
    
    // Main input loop
    char input[BUFFER_SIZE];
    while (running) {
        if (ui_get_input(input, sizeof(input))) {
            if (strncmp(input, "/exit", 5) == 0) {
                break;
            }
            SSL_write(ssl, input, strlen(input));
        }
    }
    
    // Cleanup
    running = false;
    pthread_join(recv_thread, NULL);
    cleanup();
    ui_cleanup();
    
    return EXIT_SUCCESS;
} 