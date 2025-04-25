#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <sys/select.h>

#define MAX_BUF 1024
#define SERVER_FIFO "/tmp/chat_server_fifo"
#define CLIENT_FIFO_TEMPLATE "/tmp/chat_client_%s_fifo"
#define MAX_USERS 10
#define MAX_LOG_ENTRIES 100
#define ADMIN_USERNAME "admin" // Define the admin username

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"

// Log entry type
typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_MESSAGE
} LogType;

// Log entry
typedef struct {
    char message[MAX_BUF];
    LogType type;
    time_t timestamp;
} LogEntry;

// User structure
typedef struct {
    char username[32];
    time_t last_active;
    int active;
} User;

// Server state
static int server_fifo = -1;
static User users[MAX_USERS];
static int user_count = 0;
static LogEntry log_entries[MAX_LOG_ENTRIES];
static int log_count = 0;

// Function prototypes
void cleanup();
void signal_handler(int signo);
void process_message(char *message);
void handle_join(char *username);
void handle_leave(char *username);
void handle_list_request(char *requester);
void handle_private_message(char *source, char *dest, char *content);
void forward_message(char *message);
void send_to_client(const char *username, const char *message);
void remove_inactive_users();
int find_user(const char *username);
void add_log_entry(const char *message, LogType type);
void display_dashboard();
void display_help();
void display_status();
void display_user_list();
void display_logs();
void clear_screen();
char *get_time_str(time_t t);

// Add a log entry
void add_log_entry(const char *message, LogType type) {
    if (log_count >= MAX_LOG_ENTRIES) {
        // Shift all entries up by one position
        for (int i = 0; i < MAX_LOG_ENTRIES - 1; i++) {
            log_entries[i] = log_entries[i + 1];
        }
        log_count = MAX_LOG_ENTRIES - 1;
    }
    
    strncpy(log_entries[log_count].message, message, MAX_BUF - 1);
    log_entries[log_count].message[MAX_BUF - 1] = '\0';
    log_entries[log_count].type = type;
    log_entries[log_count].timestamp = time(NULL);
    log_count++;
    
    // Only update display for critical logs or when specifically requested
    // This prevents constant screen refreshing
    if (type == LOG_ERROR || type == LOG_WARNING) {
        display_dashboard();
    }
}

// Get formatted time string
char *get_time_str(time_t t) {
    static char buf[32];
    struct tm *tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    return buf;
}

// Clear the screen
void clear_screen() {
    printf("\033[2J\033[H");  // ANSI escape sequence to clear screen and move cursor to home
}

// Display help message
void display_help() {
    printf("\n%s=== Chat Server Admin Interface ===%s\n", COLOR_CYAN, COLOR_RESET);
    printf("Commands:\n");
    printf("  %sh%s - Display this help\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sc%s - Clear the screen\n", COLOR_GREEN, COLOR_RESET);
    printf("  %su%s - Display user list\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sl%s - Display log entries\n", COLOR_GREEN, COLOR_RESET);
    printf("  %ss%s - Display server status\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sq%s - Quit server\n", COLOR_GREEN, COLOR_RESET);
    printf("Press any other key to refresh the dashboard\n\n");
}

// Display server status
void display_status() {
    printf("%s=== Server Status ===%s\n", COLOR_CYAN, COLOR_RESET);
    printf("Server FIFO: %s\n", SERVER_FIFO);
    printf("Active users: %d/%d\n", user_count, MAX_USERS);
    printf("Log entries: %d\n", log_count);
    printf("Server uptime: %ld seconds\n\n", time(NULL) - log_entries[0].timestamp);
}

// Display user list
void display_user_list() {
    printf("%s=== Active Users (%d) ===%s\n", COLOR_CYAN, user_count, COLOR_RESET);
    printf("%-20s %-15s %s\n", "Username", "Last Active", "Status");
    printf("%-20s %-15s %s\n", "--------------------", "---------------", "------");
    
    for (int i = 0; i < user_count; i++) {
        char *status = users[i].active ? COLOR_GREEN "Online" COLOR_RESET : COLOR_RED "Offline" COLOR_RESET;
        printf("%-20s %-15s %s\n", users[i].username, get_time_str(users[i].last_active), status);
    }
    printf("\n");
}

// Display logs
void display_logs() {
    printf("%s=== Recent Log Entries ===%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%-10s %-10s %s\n", "Time", "Type", "Message");
    printf("%-10s %-10s %s\n", "----------", "----------", "---------------------------------------");
    
    // Show last 10 entries or fewer
    int start = (log_count > 10) ? log_count - 10 : 0;
    
    for (int i = start; i < log_count; i++) {
        char *type_str;
        char *color;
        
        switch (log_entries[i].type) {
            case LOG_INFO:
                type_str = "INFO";
                color = COLOR_BLUE;
                break;
            case LOG_WARNING:
                type_str = "WARNING";
                color = COLOR_YELLOW;
                break;
            case LOG_ERROR:
                type_str = "ERROR";
                color = COLOR_RED;
                break;
            case LOG_MESSAGE:
                type_str = "MESSAGE";
                color = COLOR_GREEN;
                break;
            default:
                type_str = "UNKNOWN";
                color = COLOR_RESET;
                break;
        }
        
        printf("%-10s %s%-10s%s %s\n", 
               get_time_str(log_entries[i].timestamp),
               color, type_str, COLOR_RESET,
               log_entries[i].message);
    }
    printf("\n");
}

// Display the dashboard
void display_dashboard() {
    clear_screen();
    printf("%s===== Chat Server Admin Dashboard =====%s\n\n", COLOR_MAGENTA, COLOR_RESET);
    display_status();
    display_user_list();
    display_logs();
    printf("%sEnter command (h for help): %s", COLOR_GREEN, COLOR_RESET);
    fflush(stdout);
}

void cleanup() {
    if (server_fifo != -1) {
        close(server_fifo);
        server_fifo = -1;
    }
    unlink(SERVER_FIFO);
    add_log_entry("Server stopped", LOG_INFO);
}

void signal_handler(int signo) {
    char buf[MAX_BUF];
    snprintf(buf, MAX_BUF, "Received signal %d, cleaning up...", signo);
    add_log_entry(buf, LOG_WARNING);
    cleanup();
    exit(0);
}

int find_user(const char *username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && users[i].active) {
            return i;
        }
    }
    return -1;
}

// Send a message to a specific client
void send_to_client(const char *username, const char *message) {
    char client_fifo[100];
    snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMPLATE, username);
    
    fprintf(stderr, "Sending message to client %s: %s\n", username, message);
    
    int fd = open(client_fifo, O_WRONLY | O_NONBLOCK);
    if (fd == -1) {
        char error_msg[MAX_BUF];
        snprintf(error_msg, sizeof(error_msg), 
                "Failed to open client FIFO for %s: %s", username, strerror(errno));
        add_log_entry(error_msg, LOG_ERROR);
        return;
    }
    
    // Add a null terminator to ensure proper message boundary
    char *msg_with_null = malloc(strlen(message) + 1);
    if (msg_with_null == NULL) {
        char error_msg[MAX_BUF];
        snprintf(error_msg, sizeof(error_msg), 
                "Memory allocation failure when sending to %s", username);
        add_log_entry(error_msg, LOG_ERROR);
        close(fd);
        return;
    }
    
    strcpy(msg_with_null, message);
    
    ssize_t bytes_written = write(fd, msg_with_null, strlen(msg_with_null));
    if (bytes_written == -1) {
        char error_msg[MAX_BUF];
        snprintf(error_msg, sizeof(error_msg), 
                "Failed to write to client FIFO for %s: %s", username, strerror(errno));
        add_log_entry(error_msg, LOG_ERROR);
    } else {
        // Update user's last active time
        int idx = find_user(username);
        if (idx != -1) {
            users[idx].last_active = time(NULL);
        }
        
        // Log successful delivery
        char log_msg[MAX_BUF];
        snprintf(log_msg, sizeof(log_msg), "Sent message to %s (%ld bytes)", 
                username, (long)bytes_written);
        add_log_entry(log_msg, LOG_INFO);
    }
    
    free(msg_with_null);
    close(fd);
}

// Handle user joining
void handle_join(char *username) {
    char log_msg[MAX_BUF];
    
    fprintf(stderr, "User '%s' trying to join\n", username);
    
    // Check if username is already taken
    int existing_idx = find_user(username);
    if (existing_idx != -1) {
        if (users[existing_idx].active) {
            // Username already active
            snprintf(log_msg, sizeof(log_msg), 
                    "User %s tried to join but username is already active", username);
            add_log_entry(log_msg, LOG_WARNING);
            
            char error_msg[MAX_BUF];
            snprintf(error_msg, sizeof(error_msg), 
                    "SYSTEM|ERROR|%s|Username is already in use", username);
            send_to_client(username, error_msg);
            return;
        } else {
            // Reactivate existing user
            users[existing_idx].active = 1;
            users[existing_idx].last_active = time(NULL);
            
            snprintf(log_msg, sizeof(log_msg), "User %s rejoined", username);
            add_log_entry(log_msg, LOG_INFO);
        }
    } else {
        // Add new user
        if (user_count >= MAX_USERS) {
            // Server is full
            snprintf(log_msg, sizeof(log_msg), 
                    "User %s tried to join but server is full", username);
            add_log_entry(log_msg, LOG_WARNING);
            
            char error_msg[MAX_BUF];
            snprintf(error_msg, sizeof(error_msg), 
                    "SYSTEM|ERROR|%s|Server is full", username);
            send_to_client(username, error_msg);
            return;
        }
        
        fprintf(stderr, "Adding new user '%s' at index %d\n", username, user_count);
        
        // Add the new user
        strncpy(users[user_count].username, username, sizeof(users[user_count].username) - 1);
        users[user_count].username[sizeof(users[user_count].username) - 1] = '\0';
        users[user_count].active = 1;
        users[user_count].last_active = time(NULL);
        user_count++;
        
        snprintf(log_msg, sizeof(log_msg), "User %s joined", username);
        add_log_entry(log_msg, LOG_INFO);
    }
    
    // Force update dashboard to show new user
    display_dashboard();
    
    // Welcome message to the new user
    char welcome_msg[MAX_BUF];
    snprintf(welcome_msg, sizeof(welcome_msg), 
            "SYSTEM|JOIN|%s|Welcome to the chat server!", username);
    send_to_client(username, welcome_msg);
    
    // Notify other users
    char notify_msg[MAX_BUF];
    snprintf(notify_msg, sizeof(notify_msg), 
            "SYSTEM|JOIN|%s|User %s has joined the chat", username, username);
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].active && strcmp(users[i].username, username) != 0) {
            send_to_client(users[i].username, notify_msg);
        }
    }
}

// Handle user leaving
void handle_leave(char *username) {
    int idx = find_user(username);
    if (idx == -1) {
        // User not found
        char log_msg[MAX_BUF];
        snprintf(log_msg, sizeof(log_msg), 
                "Unknown user %s tried to leave", username);
        add_log_entry(log_msg, LOG_WARNING);
        return;
    }
    
    // Mark user as inactive
    users[idx].active = 0;
    
    char log_msg[MAX_BUF];
    snprintf(log_msg, sizeof(log_msg), "User %s left", username);
    add_log_entry(log_msg, LOG_INFO);
    
    // Notify other users
    char leave_msg[MAX_BUF];
    snprintf(leave_msg, sizeof(leave_msg), 
            "SYSTEM|LEAVE|%s|User %s has left the chat", username, username);
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].active && strcmp(users[i].username, username) != 0) {
            send_to_client(users[i].username, leave_msg);
        }
    }
}

// Handle request for user list
void handle_list_request(char *requester) {
    char user_list[MAX_BUF] = "";
    int count = 0;
    
    // Build list of active users
    for (int i = 0; i < user_count; i++) {
        if (users[i].active) {
            // Add comma separator if not the first user
            if (count > 0) {
                strcat(user_list, ", ");
            }
            
            // Add username to list
            strcat(user_list, users[i].username);
            count++;
        }
    }
    
    // Send user list to requester
    char message[MAX_BUF];
    snprintf(message, sizeof(message), "SYSTEM|LIST|%s|%s", requester, user_list);
    send_to_client(requester, message);
    
    // Log the request
    char log_msg[MAX_BUF];
    snprintf(log_msg, sizeof(log_msg), "User list requested by %s", requester);
    add_log_entry(log_msg, LOG_INFO);
}

// Handle private message
void handle_private_message(char *source, char *dest, char *content) {
    int dest_idx = find_user(dest);
    
    if (dest_idx == -1) {
        // Destination user not found
        char error_msg[MAX_BUF];
        snprintf(error_msg, sizeof(error_msg), 
                "SYSTEM|ERROR|%s|User %s not found or not online", source, dest);
        send_to_client(source, error_msg);
        
        char log_msg[MAX_BUF];
        snprintf(log_msg, sizeof(log_msg), 
                "Private message from %s to unknown user %s", source, dest);
        add_log_entry(log_msg, LOG_WARNING);
        return;
    }
    
    if (!users[dest_idx].active) {
        // Destination user not active
        char error_msg[MAX_BUF];
        snprintf(error_msg, sizeof(error_msg), 
                "SYSTEM|ERROR|%s|User %s is not online", source, dest);
        send_to_client(source, error_msg);
        
        char log_msg[MAX_BUF];
        snprintf(log_msg, sizeof(log_msg), 
                "Private message from %s to offline user %s", source, dest);
        add_log_entry(log_msg, LOG_WARNING);
        return;
    }
    
    // Send to destination user
    char private_msg[MAX_BUF];
    snprintf(private_msg, sizeof(private_msg), 
            "%s|PRIV|%s|%s", source, dest, content);
    send_to_client(dest, private_msg);
    
    // Echo back to sender
    snprintf(private_msg, sizeof(private_msg), 
            "%s|PRIV|%s|%s", source, dest, content);
    send_to_client(source, private_msg);
    
    // Log the message
    char log_msg[MAX_BUF];
    snprintf(log_msg, sizeof(log_msg), 
            "Private message from %s to %s: %s", source, dest, content);
    add_log_entry(log_msg, LOG_MESSAGE);
}

// Process a message from a client
void process_message(char *message) {
    if (message == NULL || strlen(message) == 0) {
        add_log_entry("Received empty message", LOG_WARNING);
        return;
    }
    
    char *msg_copy = strdup(message);
    if (msg_copy == NULL) {
        add_log_entry("Memory allocation error processing message", LOG_ERROR);
        return;
    }
    
    // Parse message format: TYPE|SOURCE|DEST|CONTENT
    char *type = msg_copy;
    char *source = NULL;
    char *dest = NULL;
    char *content = NULL;
    
    // Find first pipe
    char *pipe = strchr(type, '|');
    if (pipe != NULL) {
        *pipe = '\0';
        source = pipe + 1;
        
        // Find second pipe
        pipe = strchr(source, '|');
        if (pipe != NULL) {
            *pipe = '\0';
            dest = pipe + 1;
            
            // Find third pipe
            pipe = strchr(dest, '|');
            if (pipe != NULL) {
                *pipe = '\0';
                content = pipe + 1;
            }
        }
    }
    
    if (type == NULL || source == NULL) {
        char error_msg[MAX_BUF];
        snprintf(error_msg, sizeof(error_msg), "Invalid message format: %s", message);
        add_log_entry(error_msg, LOG_WARNING);
        free(msg_copy);
        return;
    }
    
    // Log the message
    char log_msg[MAX_BUF];
    snprintf(log_msg, sizeof(log_msg), "Received %s message from %s", type, source);
    add_log_entry(log_msg, LOG_INFO);
    
    // Process based on message type
    if (strcmp(type, "JOIN") == 0) {
        handle_join(source);
        // Only update dashboard for user joins
        display_dashboard();
    } else if (strcmp(type, "LEAVE") == 0) {
        handle_leave(source);
        // Only update dashboard for user leaves
        display_dashboard(); 
    } else if (strcmp(type, "LIST") == 0) {
        handle_list_request(source);
    } else if (strcmp(type, "MSG") == 0) {
        // Broadcast message to all users
        char formatted_msg[MAX_BUF];
        
        // Format as SOURCE|TYPE|DEST|CONTENT for clients
        snprintf(formatted_msg, sizeof(formatted_msg), "%s|MSG||%s", 
                source, content ? content : "");
        
        forward_message(formatted_msg);
        
        // Log the message
        snprintf(log_msg, sizeof(log_msg), "<%s> %s", source, content ? content : "");
        add_log_entry(log_msg, LOG_MESSAGE);
    } else if (strcmp(type, "PRIV") == 0) {
        if (dest && strlen(dest) > 0 && content) {
            handle_private_message(source, dest, content);
        } else {
            char error_msg[MAX_BUF];
            snprintf(error_msg, sizeof(error_msg), 
                    "SYSTEM|ERROR|%s|Private message format invalid", source);
            send_to_client(source, error_msg);
            add_log_entry("Invalid private message format", LOG_WARNING);
        }
    } else {
        // Unknown message type
        char error_msg[MAX_BUF];
        snprintf(error_msg, sizeof(error_msg), 
                "SYSTEM|ERROR|%s|Unknown message type: %s", source, type);
        send_to_client(source, error_msg);
        
        snprintf(log_msg, sizeof(log_msg), "Unknown message type from %s: %s", source, type);
        add_log_entry(log_msg, LOG_WARNING);
    }
    
    free(msg_copy);
    
    // Don't automatically update the dashboard after every message
    // The specific message handlers will update when needed
}

void remove_inactive_users() {
    time_t current_time = time(NULL);
    char log_msg[MAX_BUF];
    
    for (int i = 0; i < user_count; i++) {
        if (users[i].active && (current_time - users[i].last_active) > 30) {  // 30 seconds timeout
            users[i].active = 0;
            snprintf(log_msg, MAX_BUF, "User %s timed out", users[i].username);
            add_log_entry(log_msg, LOG_WARNING);
            
            // Notify all users
            char notification[MAX_BUF];
            snprintf(notification, MAX_BUF, "SYSTEM|LEAVE|%s|User %s disconnected (timeout)", 
                    users[i].username, users[i].username);
            forward_message(notification);
            
            // Clean up user's FIFO
            char client_fifo[100];
            snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMPLATE, users[i].username);
            unlink(client_fifo);
        }
    }
}

// Forward a message to all active users
void forward_message(char *message) {
    for (int i = 0; i < user_count; i++) {
        if (users[i].active) {
            send_to_client(users[i].username, message);
        }
    }
    
    char log_msg[MAX_BUF];
    snprintf(log_msg, sizeof(log_msg), "Message forwarded to %d active users", user_count);
    add_log_entry(log_msg, LOG_INFO);
}

int main() {
    char buf[MAX_BUF];
    
    // Initialize user list
    memset(users, 0, sizeof(users));
    
    // Initialize log entries
    memset(log_entries, 0, sizeof(log_entries));
    
    // Add initial log entry
    add_log_entry("Server started", LOG_INFO);

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignore SIGPIPE to prevent crash when writing to closed FIFOs

    // Create the FIFO if it doesn't exist
    if (mkfifo(SERVER_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("Failed to create server FIFO");
        add_log_entry("Failed to create server FIFO", LOG_ERROR);
        exit(EXIT_FAILURE);
    }

    add_log_entry("Server FIFO created", LOG_INFO);
    
    server_fifo = open(SERVER_FIFO, O_RDONLY | O_NONBLOCK);
    if (server_fifo == -1) {
        add_log_entry("Failed to open server FIFO", LOG_ERROR);
        unlink(SERVER_FIFO);
        exit(EXIT_FAILURE);
    }

    add_log_entry("Server FIFO opened for reading", LOG_INFO);
    
    // Open a write end to keep FIFO open
    int dummy_fifo = open(SERVER_FIFO, O_WRONLY);
    if (dummy_fifo == -1) {
        add_log_entry("Failed to open write end of server FIFO", LOG_ERROR);
        close(server_fifo);
        unlink(SERVER_FIFO);
        exit(EXIT_FAILURE);
    }

    // Display initial dashboard
    display_dashboard();
    
    // Set terminal to non-blocking input
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    fd_set read_fds;
    struct timeval tv;
    
    while (1) {
        // Set up file descriptor set for select
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(server_fifo, &read_fds);
        
        // Set timeout
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms
        
        int max_fd = (server_fifo > STDIN_FILENO) ? server_fifo : STDIN_FILENO;
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (ready == -1) {
            if (errno != EINTR) {
                add_log_entry("Select error", LOG_ERROR);
                break;
            }
            continue;
        }
        
        // Check user input
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char cmd[10] = {0};
            if (read(STDIN_FILENO, cmd, sizeof(cmd) - 1) > 0) {
                // Process command
                switch (cmd[0]) {
                    case 'h':
                    case 'H':
                        display_help();
                        break;
                    case 'c':
                    case 'C':
                        display_dashboard();
                        break;
                    case 'u':
                    case 'U':
                        display_user_list();
                        break;
                    case 'l':
                    case 'L':
                        display_logs();
                        break;
                    case 's':
                    case 'S':
                        display_status();
                        break;
                    case 'q':
                    case 'Q':
                        add_log_entry("Admin requested server shutdown", LOG_WARNING);
                        if (dummy_fifo != -1) {
                            close(dummy_fifo);
                        }
                        cleanup();
                        exit(0);
                        break;
                    default:
                        display_dashboard();
                        break;
                }
                // Clear stdin buffer by reading any remaining characters
                int c;
                while ((c = getchar()) != '\n' && c != EOF);
            }
        }
        
        // Check for FIFO messages
        if (FD_ISSET(server_fifo, &read_fds)) {
            int bytes_read = read(server_fifo, buf, MAX_BUF - 1);
            if (bytes_read > 0) {
                buf[bytes_read] = '\0';
                process_message(buf);
            } else if (bytes_read == 0) {
                // FIFO was closed by all writers
                add_log_entry("All clients disconnected. Reopening FIFO...", LOG_WARNING);
                close(server_fifo);
                server_fifo = open(SERVER_FIFO, O_RDONLY | O_NONBLOCK);
                if (server_fifo == -1) {
                    add_log_entry("Failed to reopen server FIFO", LOG_ERROR);
                    if (dummy_fifo != -1) {
                        close(dummy_fifo);
                    }
                    cleanup();
                    exit(EXIT_FAILURE);
                }
            }
        }
        
        // Periodically clean up inactive users
        remove_inactive_users();
    }

    if (dummy_fifo != -1) {
        close(dummy_fifo);
    }
    cleanup();
    return 0;
}