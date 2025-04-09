#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>

#define MAX_BUF 1024
#define SERVER_FIFO "/tmp/chat_server_fifo"
#define CLIENT_FIFO_TEMPLATE "/tmp/chat_client_%s_fifo"

// Message structure
typedef struct {
    char text[MAX_BUF];
} Message;

// Global state
static int server_fifo = -1;
static int client_fifo = -1;
static char client_fifo_name[100];
static char username[32];
static int running = 1;
static pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;
static Message messages[100];
static int message_count = 0;

// Function prototypes
void cleanup();
void signal_handler(int signo);
void *receive_messages(void *arg);
void send_join_message();
void send_leave_message();
void request_user_list();
void send_message(const char *type, const char *dest, const char *content);
void process_received_message(const char *message);
void add_message(const char *text);
int validate_username(const char *username);

// Add a message to the message list
void add_message(const char *text) {
    pthread_mutex_lock(&message_mutex);
    
    if (message_count >= 100) {
        // Shift all messages up one position
        for (int i = 0; i < 99; i++) {
            messages[i] = messages[i + 1];
        }
        message_count = 99;
    }
    
    strncpy(messages[message_count].text, text, MAX_BUF - 1);
    messages[message_count].text[MAX_BUF - 1] = '\0';
    message_count++;
    
    pthread_mutex_unlock(&message_mutex);
    
    // Display the message
    printf("%s\n", text);
}

// Send a message to the server
void send_message(const char *type, const char *dest, const char *content) {
    char message[MAX_BUF];
    snprintf(message, sizeof(message), "%s|%s|%s|%s", type, username, dest ? dest : "", content ? content : "");
    
    if (server_fifo == -1) {
        printf("Error: Not connected to server\n");
        return;
    }
    
    ssize_t bytes_written = write(server_fifo, message, strlen(message));
    if (bytes_written == -1) {
        printf("Error sending message: %s\n", strerror(errno));
        
        // Try to reconnect
        close(server_fifo);
        server_fifo = open(SERVER_FIFO, O_WRONLY);
        if (server_fifo != -1) {
            printf("Reconnected to server\n");
            // Retry sending the message
            bytes_written = write(server_fifo, message, strlen(message));
            if (bytes_written == -1) {
                printf("Error sending message after reconnect: %s\n", strerror(errno));
            }
        } else {
            printf("Failed to reconnect to server\n");
        }
    }
}

// Send a join message to the server
void send_join_message() {
    send_message("JOIN", "", "");
}

// Send a leave message to the server
void send_leave_message() {
    send_message("LEAVE", "", "");
}

// Request the user list from the server
void request_user_list() {
    send_message("LIST", "", "");
}

// Process a received message
void process_received_message(const char *message) {
    if (message == NULL || strlen(message) == 0) {
        printf("Received empty message from server\n");
        return;
    }

    char *msg_copy = strdup(message);
    if (msg_copy == NULL) {
        printf("Memory allocation error processing message\n");
        return;
    }
    
    // Parse message format: SOURCE\nTYPE\nDEST\nCONTENT
    char *source = msg_copy;
    char *type = NULL;
    char *dest = NULL;
    char *content = NULL;
    
    // Find first newline
    char *newline = strchr(source, '\n');
    if (newline != NULL) {
        *newline = '\0';
        type = newline + 1;
        
        // Find second newline
        newline = strchr(type, '\n');
        if (newline != NULL) {
            *newline = '\0';
            dest = newline + 1;
            
            // Find third newline
            newline = strchr(dest, '\n');
            if (newline != NULL) {
                *newline = '\0';
                content = newline + 1;
            }
        }
    }

    if (source && type) {
        if (strcmp(source, "SYSTEM") == 0) {
            if (strcmp(type, "JOIN") == 0) {
                printf("User %s joined the chat\n", dest);
            } else if (strcmp(type, "LEAVE") == 0) {
                printf("User %s left the chat\n", dest);
            } else if (strcmp(type, "LIST") == 0) {
                printf("Online users: %s\n", content ? content : "none");
            } else if (strcmp(type, "ERROR") == 0) {
                printf("Server error: %s\n", content ? content : "Unknown error");
            }
        } else if (strcmp(type, "MSG") == 0) {
            // Regular broadcast message
            printf("<%s> %s\n", source, content ? content : "");
        } else if (strcmp(type, "PRIV") == 0) {
            // Private message
            if (strcmp(source, username) == 0) {
                printf("To <%s>: %s\n", dest, content ? content : "");
            } else {
                printf("From <%s>: %s\n", source, content ? content : "");
            }
        } else {
            // Unknown message type
            printf("Unknown message type from %s: %s\n", source, type);
        }
    } else {
        // Invalid message format
        printf("Invalid message format from server\n");
    }
    
    free(msg_copy);
}

// Receive thread function
void *receive_messages(void *arg) {
    (void)arg;  // Mark parameter as intentionally unused
    char buf[MAX_BUF];
    
    while (running) {
        // Read from client FIFO
        int bytes_read = read(client_fifo, buf, sizeof(buf) - 1);
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            process_received_message(buf);
        } else if (bytes_read == 0) {
            // FIFO closed, try to reopen
            close(client_fifo);
            client_fifo = open(client_fifo_name, O_RDONLY | O_NONBLOCK);
            if (client_fifo == -1) {
                printf("Lost connection to server\n");
                if (!running) break;
                sleep(1); // Don't try to reconnect too aggressively
            }
        } else if (errno != EAGAIN) {
            // Error reading from FIFO
            printf("Error reading from server: %s\n", strerror(errno));
        }
        
        usleep(100000); // Sleep for 100ms to avoid high CPU usage
    }
    
    return NULL;
}

// Clean up resources
void cleanup() {
    running = 0;
    
    // Send leave message
    if (server_fifo != -1) {
        send_leave_message();
        close(server_fifo);
        server_fifo = -1;
    }
    
    // Clean up client FIFO
    if (client_fifo != -1) {
        close(client_fifo);
        client_fifo = -1;
    }
    unlink(client_fifo_name);
}

// Signal handler
void signal_handler(int signo) {
    printf("\nReceived signal %d, exiting...\n", signo);
    cleanup();
    exit(0);
}

// Validate a username
int validate_username(const char *username) {
    if (username == NULL) return 0;
    
    size_t len = strlen(username);
    if (len < 3 || len > 31) return 0;
    
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            return 0;
        }
    }
    
    return 1;
}

// Connect to the server
int connect_to_server() {
    // Open server FIFO for writing
    server_fifo = open(SERVER_FIFO, O_WRONLY);
    if (server_fifo == -1) {
        printf("Cannot connect to server: %s\n", strerror(errno));
        return 0;
    }
    
    // Create client FIFO
    snprintf(client_fifo_name, sizeof(client_fifo_name), CLIENT_FIFO_TEMPLATE, username);
    unlink(client_fifo_name); // Remove if it already exists
    
    if (mkfifo(client_fifo_name, 0666) == -1) {
        printf("Cannot create client FIFO: %s\n", strerror(errno));
        close(server_fifo);
        server_fifo = -1;
        return 0;
    }
    
    // Open client FIFO for reading
    client_fifo = open(client_fifo_name, O_RDONLY | O_NONBLOCK);
    if (client_fifo == -1) {
        printf("Cannot open client FIFO for reading: %s\n", strerror(errno));
        close(server_fifo);
        server_fifo = -1;
        unlink(client_fifo_name);
        return 0;
    }
    
    // Send join message to server
    send_join_message();
    printf("Connected to server\n");
    
    return 1;
}

int main() {
    char input[MAX_BUF];
    pthread_t receive_thread;
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignore SIGPIPE to prevent crash when writing to closed FIFOs
    
    // Get username
    printf("Enter your username (3-31 alphanumeric characters): ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0; // Remove newline
    
    while (!validate_username(username)) {
        printf("Invalid username. Please use 3-31 alphanumeric characters or underscores.\n");
        printf("Enter your username: ");
        fgets(username, sizeof(username), stdin);
        username[strcspn(username, "\n")] = 0;
    }
    
    // Connect to server
    if (!connect_to_server()) {
        printf("Failed to connect to server. Exiting.\n");
        return 1;
    }
    
    // Start receive thread
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        printf("Error creating receive thread\n");
        cleanup();
        return 1;
    }
    
    // Show help
    printf("\nCommands:\n");
    printf("  /help - Show this help\n");
    printf("  /list - List online users\n");
    printf("  /msg <username> <message> - Send private message\n");
    printf("  /quit - Exit the chat\n");
    printf("Type anything else to send a broadcast message\n\n");
    
    // Request user list
    request_user_list();
    
    // Main loop
    while (running) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        // Check for empty input
        if (strlen(input) == 0) {
            continue;
        }
        
        // Check for commands
        if (input[0] == '/') {
            if (strcmp(input, "/quit") == 0 || strcmp(input, "/exit") == 0) {
                break;
            } else if (strcmp(input, "/help") == 0) {
                printf("\nCommands:\n");
                printf("  /help - Show this help\n");
                printf("  /list - List online users\n");
                printf("  /msg <username> <message> - Send private message\n");
                printf("  /quit - Exit the chat\n");
                printf("Type anything else to send a broadcast message\n\n");
            } else if (strcmp(input, "/list") == 0) {
                request_user_list();
            } else if (strncmp(input, "/msg ", 5) == 0) {
                char *dest = input + 5;
                char *message = strchr(dest, ' ');
                if (message != NULL) {
                    *message = '\0';
                    message++;
                    send_message("PRIV", dest, message);
                } else {
                    printf("Usage: /msg <username> <message>\n");
                }
            } else {
                printf("Unknown command. Type /help for available commands.\n");
            }
        } else {
            // Send as broadcast message
            send_message("MSG", "ALL", input);
        }
    }
    
    // Clean up
    cleanup();
    
    // Wait for receive thread to finish
    if (pthread_join(receive_thread, NULL) != 0) {
        printf("Error waiting for receive thread\n");
    }
    
    printf("Goodbye!\n");
    return 0;
}