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
#include "network_config.h"
#include "security.h"

#define BUFFER_SIZE 4096
#define NAME_LENGTH 32
#define MAX_FILE_SIZE (10 * 1024 * 1024) // 10MB

// Global variables
int client_socket;
bool connected = false;
bool authenticated = false;
char username[NAME_LENGTH];
bool running = true;
int current_room = 0;
auth_data_t auth;

// Function prototypes
void *receive_messages(void *arg);
void handle_signal(int sig);
void connect_to_server(const char *server_ip, int server_port);
void authenticate(const char *username);
void cleanup();
void show_help();
void handle_file_receive(char *message);
void send_file(const char *recipient, const char *filepath);

// Show available commands
void show_help() {
    printf("\nAvailable commands:\n");
    printf("/msg <username> <message> - Send private message\n");
    printf("/status <online|away|busy> [message] - Change status\n");
    printf("/join <room_id> - Join a chat room\n");
    printf("/create <room_name> - Create a new chat room\n");
    printf("/file <username> <filepath> - Send file to user\n");
    printf("/list - Show online users\n");
    printf("/help - Show this help message\n");
    printf("/exit - Exit the chat\n\n");
}

// Handle received file
void handle_file_receive(char *message) {
    char sender[NAME_LENGTH];
    char filename[256];
    char filepath[256];
    
    // Parse file notification
    if (sscanf(message, "FILE:%[^:]:%[^:]:%s", sender, filename, filepath) != 3) {
        printf("Invalid file notification format\n");
        return;
    }
    
    printf("\nReceived file '%s' from %s\n", filename, sender);
    printf("File saved as: %s\n", filepath);
}

// Send file to user
void send_file(const char *recipient, const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        printf("File not found: %s\n", filepath);
        return;
    }
    
    if (st.st_size > MAX_FILE_SIZE) {
        printf("File too large (max size: 10MB)\n");
        return;
    }
    
    // Open file
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        printf("Failed to open file: %s\n", filepath);
        return;
    }
    
    // Get filename from path
    const char *filename = strrchr(filepath, '/');
    if (filename) {
        filename++; // Skip '/'
    } else {
        filename = filepath;
    }
    
    // Send file command
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "/file %s %s", recipient, filename);
    send(client_socket, command, strlen(command), 0);
    
    // Send file content
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) < 0) {
            printf("Failed to send file\n");
            break;
        }
    }
    
    fclose(fp);
    printf("File sent successfully\n");
}

// Connect to chat server
void connect_to_server(const char *server_ip, int server_port) {
    struct sockaddr_in server_addr;
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure TCP socket
    if (configure_tcp_socket(client_socket) < 0) {
        perror("Failed to configure socket");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    printf("Connecting to server %s:%d...\n", server_ip, server_port);
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server.\n");
    connected = true;
}

// Authenticate with server
void authenticate(const char *username) {
    if (!connected) return;
    
    // Initialize authentication data
    strncpy(auth.username, username, sizeof(auth.username) - 1);
    
    // Generate salt and token
    if (generate_salt(auth.salt) < 0) {
        printf("Failed to generate salt\n");
        return;
    }
    
    if (generate_token(auth.token) < 0) {
        printf("Failed to generate token\n");
        return;
    }
    
    auth.token_expiry = time(NULL) + 3600; // Token expires in 1 hour
    
    // Send username to server
    if (send(client_socket, username, strlen(username), 0) < 0) {
        perror("Failed to send username");
        return;
    }
    
    authenticated = true;
    printf("Username sent to server.\n");
}

// Thread function to receive messages
void *receive_messages(void *arg) {
    (void)arg; // Suppress unused parameter warning
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    printf("Message receiver started.\n");
    
    while (running && connected) {
        memset(buffer, 0, BUFFER_SIZE);
        
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            // Check for special messages
            if (strncmp(buffer, "FILE:", 5) == 0) {
                handle_file_receive(buffer);
            }
            else if (strncmp(buffer, "ROOM:", 5) == 0) {
                // Room related messages
                printf("\n%s\n", buffer + 5);
            }
            else {
                // Regular message
                printf("%s\n", buffer);
            }
        }
        else if (bytes_read == 0) {
            printf("Disconnected from server.\n");
            connected = false;
            running = false;
            break;
        }
        else {
            if (running) {
                perror("Receive failed");
                connected = false;
                running = false;
                break;
            }
        }
        
        usleep(100000); // 100ms delay
    }
    
    return NULL;
}

// Handle signals (Ctrl+C)
void handle_signal(int sig) {
    printf("\nReceived signal %d, disconnecting...\n", sig);
    running = false;
}

// Clean up resources
void cleanup() {
    if (client_socket >= 0) {
        close(client_socket);
        client_socket = -1;
    }
    connected = false;
    authenticated = false;
}

// Main function
int main(int argc, char *argv[]) {
    // Default server settings
    char server_ip[16] = "127.0.0.1";
    int server_port = 8990;
    
    // Parse command line arguments
    if (argc >= 2) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
    }
    
    if (argc >= 3) {
        server_port = atoi(argv[2]);
    }
    
    // Setup signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Connect to server
    connect_to_server(server_ip, server_port);
    
    // Get username
    printf("Enter your username: ");
    fgets(username, NAME_LENGTH, stdin);
    
    // Remove newline
    size_t len = strlen(username);
    if (len > 0 && username[len - 1] == '\n') {
        username[len - 1] = '\0';
    }
    
    // Authenticate with server
    authenticate(username);
    
    // Start message receiving thread
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("Failed to create thread");
        cleanup();
        return EXIT_FAILURE;
    }
    
    // Show help message
    show_help();
    
    // Main message loop
    char message[BUFFER_SIZE];
    printf("Start typing messages or commands:\n");
    
    while (running) {
        if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Remove newline
        len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') {
            message[len - 1] = '\0';
        }
        
        // Check for commands
        if (message[0] == '/') {
            if (strcmp(message, "/exit") == 0) {
                running = false;
                break;
            }
            else if (strcmp(message, "/help") == 0) {
                show_help();
                continue;
            }
            else if (strncmp(message, "/file", 5) == 0) {
                char recipient[NAME_LENGTH];
                char filepath[256];
                if (sscanf(message, "/file %s %s", recipient, filepath) == 2) {
                    send_file(recipient, filepath);
                } else {
                    printf("Usage: /file <username> <filepath>\n");
                }
                continue;
            }
        }
        
        // Send message to server
        if (connected && authenticated && send(client_socket, message, strlen(message), 0) < 0) {
            perror("Send failed");
            break;
        }
    }
    
    // Wait for receive thread to finish
    pthread_join(recv_thread, NULL);
    
    // Cleanup and exit
    cleanup();
    printf("Disconnected from server.\n");
    return EXIT_SUCCESS;
} 