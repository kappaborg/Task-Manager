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
    
    // Update display
    display_dashboard();
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
    
    int fd = open(client_fifo, O_WRONLY | O_NONBLOCK);
    if (fd != -1) {
        ssize_t bytes_written = write(fd, message, strlen(message));
        close(fd);
        
        if (bytes_written == -1) {
            char log_msg[MAX_BUF];
            snprintf(log_msg, MAX_BUF, "Failed to write to client FIFO for %s: %s", 
                     username, strerror(errno));
            add_log_entry(log_msg, LOG_ERROR);
            return;
        }
        
        char log_msg[MAX_BUF];
        snprintf(log_msg, MAX_BUF, "Sent to %s: %s", username, message);
        add_log_entry(log_msg, LOG_INFO);
    } else {
        char log_msg[MAX_BUF];
        snprintf(log_msg, MAX_BUF, "Failed to open client FIFO for %s: %s", 
                 username, strerror(errno));
        add_log_entry(log_msg, LOG_ERROR);
    }
}

void handle_join(char *username) {
    int idx = find_user(username);
    char log_msg[MAX_BUF];
    char client_fifo[100];
    
    // Check if username is valid
    if (strlen(username) < 3 || strlen(username) > 31) {
        snprintf(log_msg, MAX_BUF, "Rejected join request from %s: Invalid username length", username);
        add_log_entry(log_msg, LOG_WARNING);
        return;
    }

    // Check if username contains only valid characters
    for (size_t i = 0; i < strlen(username); i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            snprintf(log_msg, MAX_BUF, "Rejected join request from %s: Invalid characters in username", username);
            add_log_entry(log_msg, LOG_WARNING);
            return;
        }
    }
    
    if (idx != -1) {
        // User already exists, update their status
        users[idx].active = 1;
        users[idx].last_active = time(NULL);
        snprintf(log_msg, MAX_BUF, "User %s reconnected", username);
        add_log_entry(log_msg, LOG_INFO);
    } else if (user_count < MAX_USERS) {
        // Add new user
        strncpy(users[user_count].username, username, sizeof(users[user_count].username) - 1);
        users[user_count].username[sizeof(users[user_count].username) - 1] = '\0';
        users[user_count].active = 1;
        users[user_count].last_active = time(NULL);
        user_count++;
        
        snprintf(log_msg, MAX_BUF, "User %s joined", username);
        add_log_entry(log_msg, LOG_INFO);
        
        // Notify all users about the new user
        char notification[MAX_BUF];
        snprintf(notification, MAX_BUF, "SYSTEM\nJOIN\n%s\nUser %s joined the chat", username, username);
        forward_message(notification);
    } else {
        snprintf(log_msg, MAX_BUF, "Rejected join request from %s: Server full", username);
        add_log_entry(log_msg, LOG_WARNING);
        
        // Notify the user that the server is full
        snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMPLATE, username);
        int fd = open(client_fifo, O_WRONLY | O_NONBLOCK);
        if (fd != -1) {
            char msg[] = "SYSTEM\nERROR\n\nServer is full";
            write(fd, msg, strlen(msg));
            close(fd);
        }
    }
}

void handle_leave(char *username) {
    int idx = find_user(username);
    char log_msg[MAX_BUF];
    
    if (idx != -1) {
        users[idx].active = 0;
        snprintf(log_msg, MAX_BUF, "User %s left", username);
        add_log_entry(log_msg, LOG_INFO);
        
        // Notify all users
        char notification[MAX_BUF];
        snprintf(notification, MAX_BUF, "SYSTEM\nLEAVE\n%s\nUser %s left the chat", username, username);
        forward_message(notification);
        
        // Clean up user's FIFO
        char client_fifo[100];
        snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMPLATE, username);
        unlink(client_fifo);
    }
}

void handle_list_request(char *requester) {
    char list[MAX_BUF] = "";
    char *current = list;
    int remaining = MAX_BUF;
    int first = 1;
    
    // Build list of active users
    for (int i = 0; i < user_count; i++) {
        if (users[i].active) {
            int written;
            if (first) {
                written = snprintf(current, remaining, "%s", users[i].username);
                first = 0;
            } else {
                written = snprintf(current, remaining, ",%s", users[i].username);
            }
            
            if (written >= remaining) break;  // Buffer full
            current += written;
            remaining -= written;
        }
    }
    
    // Send the list to the requester
    char response[MAX_BUF];
    snprintf(response, MAX_BUF, "SYSTEM\nLIST\n%s\n%s", requester, list);
    send_to_client(requester, response);
}

void handle_private_message(char *source, char *dest, char *content) {
    char log_msg[MAX_BUF];
    snprintf(log_msg, MAX_BUF, "Private message from %s to %s: %s", source, dest, content);
    add_log_entry(log_msg, LOG_MESSAGE);
    
    // Parse destination users (comma separated list)
    char dest_copy[MAX_BUF];
    strncpy(dest_copy, dest, MAX_BUF-1);
    dest_copy[MAX_BUF-1] = '\0';
    
    char message[MAX_BUF];
    snprintf(message, MAX_BUF, "PRIV|%s|%s|%s", source, dest, content);
    
    char *token = strtok(dest_copy, ",");
    while (token != NULL) {
        int idx = find_user(token);
        if (idx != -1) {
            // Forward to the recipient
            send_to_client(token, message);
        }
        token = strtok(NULL, ",");
    }
    
    // Also send back to the sender
    send_to_client(source, message);
}

void process_message(char *message) {
    char type[10] = {0};
    char source[32] = {0};
    char dest[MAX_BUF] = {0};
    char content[MAX_BUF] = {0};
    
    // Add raw message to log
    char log_msg[MAX_BUF];
    snprintf(log_msg, MAX_BUF, "Received: %s", message);
    add_log_entry(log_msg, LOG_INFO);
    
    // Parse message: TYPE|SOURCE|DEST|CONTENT
    // Format is TYPE|SOURCE|DEST|CONTENT, but DEST and CONTENT might be empty
    int parsed = 0;
    char *msg_copy = NULL;
    
    if (message != NULL) {
        msg_copy = strdup(message);
        if (msg_copy == NULL) {
            add_log_entry("Failed to allocate memory for message parsing", LOG_ERROR);
            return;
        }
    } else {
        add_log_entry("Received NULL message", LOG_ERROR);
        return;
    }
    
    char *token;
    int field = 0;
    
    token = strtok(msg_copy, "|");
    while (token != NULL && field < 4) {
        switch (field) {
            case 0: // type
                strncpy(type, token, sizeof(type) - 1);
                break;
            case 1: // source
                strncpy(source, token, sizeof(source) - 1);
                break;
            case 2: // dest
                strncpy(dest, token, sizeof(dest) - 1);
                break;
            case 3: // content
                strncpy(content, token, sizeof(content) - 1);
                break;
        }
        field++;
        
        // For content, get the rest of the message
        if (field == 3) {
            token = strtok(NULL, "");
            if (token != NULL) {
                strncpy(content, token, sizeof(content) - 1);
            }
            break;
        } else {
            token = strtok(NULL, "|");
        }
    }
    
    parsed = field;
    
    // Log parsed message
    char log_msg2[MAX_BUF];
    snprintf(log_msg2, MAX_BUF, "Parsed: type=[%s], source=[%s], dest=[%s], content=[%s]", 
           type, source, dest, content);
    add_log_entry(log_msg2, LOG_INFO);
    
    // We need at least type and source fields
    if (parsed >= 2) {
        if (strcmp(type, "JOIN") == 0) {
            handle_join(source);
        } else if (strcmp(type, "LEAVE") == 0) {
            handle_leave(source);
        } else if (strcmp(type, "LIST") == 0) {
            handle_list_request(source);
        } else if (strcmp(type, "MSG") == 0) {
            // Broadcast message
            char log_msg3[MAX_BUF];
            snprintf(log_msg3, MAX_BUF, "Broadcast from %s: %s", source, content);
            add_log_entry(log_msg3, LOG_MESSAGE);
            
            // Forward to all active users
            char broadcast[MAX_BUF];
            snprintf(broadcast, MAX_BUF, "MSG|%s|ALL|%s", source, content);
            
            for (int i = 0; i < user_count; i++) {
                if (users[i].active && strcmp(users[i].username, source) != 0) {
                    send_to_client(users[i].username, broadcast);
                }
            }
        } else if (strcmp(type, "PRIV") == 0) {
            handle_private_message(source, dest, content);
        }
        
        // Update user's last active time
        int idx = find_user(source);
        if (idx != -1) {
            users[idx].last_active = time(NULL);
        }
    } else {
        char log_msg3[MAX_BUF];
        snprintf(log_msg3, MAX_BUF, "Invalid message format (parsed %d fields): %s", parsed, message);
        add_log_entry(log_msg3, LOG_ERROR);
    }
    
    free(msg_copy); // Free the allocated memory
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
            snprintf(notification, MAX_BUF, "SYSTEM\nLEAVE\n%s\nUser %s disconnected (timeout)", 
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
    char log_msg[MAX_BUF];
    snprintf(log_msg, MAX_BUF, "Broadcasting: %s", message);
    add_log_entry(log_msg, LOG_INFO);
    
    // Send to all active users
    for (int i = 0; i < user_count; i++) {
        if (users[i].active) {
            send_to_client(users[i].username, message);
        }
    }
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