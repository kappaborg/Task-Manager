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
#include <ncurses.h>
#include <ctype.h>
#include <locale.h>

#define MAX_BUF 1024
#define SERVER_FIFO "/tmp/chat_server_fifo"
#define CLIENT_FIFO_TEMPLATE "/tmp/chat_client_%s_fifo"
#define MAX_USERS 10
#define MAX_MESSAGES 100
#define INPUT_HEIGHT 3
#define USER_LIST_WIDTH 20

// TUI specific defines
#define COLOR_NORMAL 1
#define COLOR_SYSTEM 2
#define COLOR_ERROR 3
#define COLOR_PRIVATE 4
#define COLOR_HIGHLIGHT 5
#define COLOR_USER_LIST 6

// Message types
enum MessageType {
    MSG_NORMAL,
    MSG_SYSTEM,
    MSG_ERROR,
    MSG_PRIVATE
};

// Message structure
typedef struct {
    char text[MAX_BUF];
    enum MessageType type;
} Message;

// Windows
WINDOW *message_win = NULL;
WINDOW *input_win = NULL;
WINDOW *user_list_win = NULL;

// Global state
static int server_fifo = -1;
static int client_fifo = -1;
static char client_fifo_name[100];
static char username[32];
static int running = 1;
static char user_list[MAX_USERS][32];
static int user_count = 0;
static pthread_mutex_t user_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;
static Message messages[MAX_MESSAGES];
static int message_count = 0;
static int message_scroll = 0;
static int current_mode = 0; // 0: normal, 1: command, 2: user selection
static int selected_user = -1;
static char input_buffer[MAX_BUF] = "";
static int input_pos = 0;
static int command_mode = 0;

pthread_mutex_t resize_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t resize_required = 0;

// Function prototypes
void cleanup();
void signal_handler(int signo);
void *receive_messages(void *arg);
void send_join_message();
void send_leave_message();
void request_user_list();
void send_message(const char *type, const char *dest, const char *content);
void handle_user_list(const char *list);
void process_received_message(const char *message);
void add_message(const char *text, enum MessageType type);
void handle_server_message(const char *type, const char *source, const char *dest, const char *content);
void init_ui();
void cleanup_ui();
void draw_user_list();
void draw_messages();
void draw_input();
void draw_status_bar();
void process_input(int ch);
void select_user(int direction);
void handle_command(const char *cmd);
void update_ui();
int validate_username(const char *username);
void get_username(void);
void handle_resize(int signo);
int connect_to_server(void);
void resize_windows();

// Initialize ncurses UI
void init_ui() {
    setlocale(LC_ALL, "");  // Add locale support for proper UTF-8 handling
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(100); // Non-blocking input
    curs_set(1);  // Show cursor
    
    // Initialize color pairs
    init_pair(COLOR_NORMAL, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_SYSTEM, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_ERROR, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_PRIVATE, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_USER_LIST, COLOR_CYAN, COLOR_BLACK);
    
    // Create windows
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    user_list_win = newwin(max_y - 1, USER_LIST_WIDTH, 0, 0);
    message_win = newwin(max_y - INPUT_HEIGHT - 1, max_x - USER_LIST_WIDTH, 0, USER_LIST_WIDTH);
    input_win = newwin(INPUT_HEIGHT, max_x, max_y - INPUT_HEIGHT, 0);
    
    scrollok(message_win, TRUE);
    
    // Draw initial UI
    draw_user_list();
    draw_messages();
    draw_input();
    draw_status_bar();
    
    // Add welcome message
    add_message("Welcome to Chat TUI Client", MSG_SYSTEM);
    add_message("Press F1 for help", MSG_SYSTEM);
}

// Clean up ncurses UI
void cleanup_ui() {
    delwin(message_win);
    delwin(input_win);
    delwin(user_list_win);
    endwin();
}

// Draw user list panel
void draw_user_list() {
    pthread_mutex_lock(&user_list_mutex);
    
    werase(user_list_win);
    box(user_list_win, 0, 0);
    
    int win_height, win_width;
    getmaxyx(user_list_win, win_height, win_width);
    
    mvwprintw(user_list_win, 0, (win_width - 10) / 2, " Users (%d) ", user_count);
    
    for (int i = 0; i < user_count && i < win_height - 2; i++) {
        if (i == selected_user && current_mode == 2) {
            wattron(user_list_win, COLOR_PAIR(COLOR_HIGHLIGHT));
            mvwhline(user_list_win, i + 1, 1, ' ', win_width - 2);
            mvwprintw(user_list_win, i + 1, 2, "%s", user_list[i]);
            wattroff(user_list_win, COLOR_PAIR(COLOR_HIGHLIGHT));
        } else {
            wattron(user_list_win, COLOR_PAIR(COLOR_USER_LIST));
            mvwprintw(user_list_win, i + 1, 2, "%s", user_list[i]);
            wattroff(user_list_win, COLOR_PAIR(COLOR_USER_LIST));
        }
    }
    
    wrefresh(user_list_win);
    pthread_mutex_unlock(&user_list_mutex);
}

// Draw message panel
void draw_messages() {
    pthread_mutex_lock(&message_mutex);
    
    werase(message_win);
    box(message_win, 0, 0);
    
    int win_height, win_width;
    getmaxyx(message_win, win_height, win_width);
    
    mvwprintw(message_win, 0, (win_width - 10) / 2, " Messages ");
    
    int display_count = win_height - 2;
    int start = (message_count > display_count) ? message_count - display_count - message_scroll : 0;
    
    if (start < 0) start = 0;
    
    for (int i = 0; i < display_count && start + i < message_count; i++) {
        Message *msg = &messages[start + i];
        
        switch (msg->type) {
            case MSG_SYSTEM:
                wattron(message_win, COLOR_PAIR(COLOR_SYSTEM));
                break;
            case MSG_ERROR:
                wattron(message_win, COLOR_PAIR(COLOR_ERROR));
                break;
            case MSG_PRIVATE:
                wattron(message_win, COLOR_PAIR(COLOR_PRIVATE));
                break;
            default:
                wattron(message_win, COLOR_PAIR(COLOR_NORMAL));
                break;
        }
        
        mvwprintw(message_win, i + 1, 1, "%.*s", win_width - 2, msg->text);
        
        switch (msg->type) {
            case MSG_SYSTEM:
                wattroff(message_win, COLOR_PAIR(COLOR_SYSTEM));
                break;
            case MSG_ERROR:
                wattroff(message_win, COLOR_PAIR(COLOR_ERROR));
                break;
            case MSG_PRIVATE:
                wattroff(message_win, COLOR_PAIR(COLOR_PRIVATE));
                break;
            default:
                wattroff(message_win, COLOR_PAIR(COLOR_NORMAL));
                break;
        }
    }
    
    wrefresh(message_win);
    pthread_mutex_unlock(&message_mutex);
}

// Draw input panel
void draw_input() {
    werase(input_win);
    box(input_win, 0, 0);
    
    int ignored;
    int win_width;
    getmaxyx(input_win, ignored, win_width); // Store unused height in ignored variable
    (void)ignored; // Explicitly mark as unused to suppress warning
    
    const char *prompt = command_mode ? "Command: " : "Message: ";
    mvwprintw(input_win, 0, (win_width - 8) / 2, " Input ");
    mvwprintw(input_win, 1, 1, "%s%s", prompt, input_buffer);
    
    // Position cursor
    wmove(input_win, 1, strlen(prompt) + input_pos);
    
    wrefresh(input_win);
}

// Draw status bar
void draw_status_bar() {
    attron(A_REVERSE);
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    char status[MAX_BUF];
    
    if (current_mode == 0) {
        sprintf(status, " Chat | User: %s | F1: Help | ESC: Menu | ←→: Users | /cmd: Commands ", username);
    } else if (current_mode == 1) {
        sprintf(status, " COMMAND MODE | TAB: Users | ESC: Cancel ");
    } else if (current_mode == 2) {
        sprintf(status, " USER SELECTION | ↑↓: Navigate | ENTER: Select | ESC: Cancel ");
    }
    
    mvhline(max_y - INPUT_HEIGHT - 1, 0, ' ', max_x);
    mvprintw(max_y - INPUT_HEIGHT - 1, 0, "%.*s", max_x, status);
    
    attroff(A_REVERSE);
    refresh();
}

// Add a new message to the list
void add_message(const char *text, enum MessageType type) {
    pthread_mutex_lock(&message_mutex);
    
    if (message_count >= MAX_MESSAGES) {
        // Shift all messages up one position
        for (int i = 0; i < MAX_MESSAGES - 1; i++) {
            messages[i] = messages[i + 1];
        }
        message_count = MAX_MESSAGES - 1;
    }
    
    strncpy(messages[message_count].text, text, MAX_BUF - 1);
    messages[message_count].text[MAX_BUF - 1] = '\0';
    messages[message_count].type = type;
    message_count++;
    
    pthread_mutex_unlock(&message_mutex);
    
    // Redraw message window
    draw_messages();
}

// Process a key press
void process_input(int ch) {
    if (current_mode == 2) {
        // User selection mode
        switch (ch) {
            case KEY_UP:
                select_user(-1);
                break;
            case KEY_DOWN:
                select_user(1);
                break;
            case '\n':
                if (selected_user >= 0 && selected_user < user_count) {
                    char *dest = user_list[selected_user];
                    snprintf(input_buffer, MAX_BUF, "/msg %s ", dest);
                    input_pos = strlen(input_buffer);
                    current_mode = 0;
                    draw_status_bar();
                    draw_input();
                }
                break;
            case 27: // ESC
                current_mode = 0;
                selected_user = -1;
                draw_status_bar();
                draw_user_list();
                break;
        }
        return;
    }
    
    switch (ch) {
        case KEY_F(1):
            add_message("=== HELP ===", MSG_SYSTEM);
            add_message("F1: Show help", MSG_SYSTEM);
            add_message("ESC: Toggle command mode", MSG_SYSTEM);
            add_message("TAB: Select a user", MSG_SYSTEM);
            add_message("LEFT/RIGHT: Enter user selection mode", MSG_SYSTEM);
            add_message("UP/DOWN: Navigate messages or users", MSG_SYSTEM);
            add_message("/list: Request user list", MSG_SYSTEM);
            add_message("/msg <user> <message>: Send private message", MSG_SYSTEM);
            add_message("/help: Show this help", MSG_SYSTEM);
            add_message("/quit: Exit the chat", MSG_SYSTEM);
            break;
        
        case 27: // ESC
            if (current_mode == 0) {
                current_mode = 1;
                command_mode = 1;
                input_buffer[0] = '/';
                input_buffer[1] = '\0';
                input_pos = 1;
            } else {
                current_mode = 0;
                command_mode = 0;
                input_buffer[0] = '\0';
                input_pos = 0;
            }
            draw_status_bar();
            draw_input();
            break;
        
        case KEY_BACKSPACE:
        case 127:
            if (input_pos > 0) {
                memmove(&input_buffer[input_pos - 1], &input_buffer[input_pos], 
                        strlen(input_buffer) - input_pos + 1);
                input_pos--;
                draw_input();
            }
            break;
        
        case KEY_DC:
            if (input_pos < (int)strlen(input_buffer)) {
                memmove(&input_buffer[input_pos], &input_buffer[input_pos + 1], 
                        strlen(input_buffer) - input_pos);
                draw_input();
            }
            break;
        
        case KEY_LEFT:
            if (current_mode == 0 && user_count > 0) {
                // Enter user selection mode and select the last user
                current_mode = 2;
                selected_user = user_count - 1;
                draw_status_bar();
                draw_user_list();
            } else if (input_pos > 0) {
                input_pos--;
                draw_input();
            }
            break;
        
        case KEY_RIGHT:
            if (current_mode == 0 && user_count > 0) {
                // Enter user selection mode and select the first user
                current_mode = 2;
                selected_user = 0;
                draw_status_bar();
                draw_user_list();
            } else if (input_pos < (int)strlen(input_buffer)) {
                input_pos++;
                draw_input();
            }
            break;
        
        case KEY_HOME:
            input_pos = 0;
            draw_input();
            break;
        
        case KEY_END:
            input_pos = strlen(input_buffer);
            draw_input();
            break;
        
        case KEY_UP:
            message_scroll++;
            draw_messages();
            break;
        
        case KEY_DOWN:
            if (message_scroll > 0) {
                message_scroll--;
                draw_messages();
            }
            break;
        
        case '\t':
            current_mode = 2;
            selected_user = 0;
            draw_status_bar();
            draw_user_list();
            break;
        
        case '\n':
            if (strlen(input_buffer) > 0) {
                // Special handling for commands
                if (input_buffer[0] == '/') {
                    handle_command(input_buffer);
                } else {
                    // Regular message
                    send_message("MSG", "", input_buffer);
                    add_message(input_buffer, MSG_NORMAL);
                }
                
                input_buffer[0] = '\0';
                input_pos = 0;
                command_mode = 0;
                current_mode = 0;
                draw_status_bar();
                draw_input();
            }
            break;
        
        default:
            if (ch >= 32 && ch <= 126 && strlen(input_buffer) < MAX_BUF - 1) {
                memmove(&input_buffer[input_pos + 1], &input_buffer[input_pos], 
                        strlen(input_buffer) - input_pos + 1);
                input_buffer[input_pos] = ch;
                input_pos++;
                draw_input();
            }
            break;
    }
}

// Handle command input
void handle_command(const char *cmd) {
    if (strcmp(cmd, "/help") == 0) {
        add_message("=== HELP ===", MSG_SYSTEM);
        add_message("F1: Show help", MSG_SYSTEM);
        add_message("ESC: Toggle command mode", MSG_SYSTEM);
        add_message("TAB: Select a user", MSG_SYSTEM);
        add_message("LEFT/RIGHT: Enter user selection mode", MSG_SYSTEM);
        add_message("UP/DOWN: Navigate messages or users", MSG_SYSTEM);
        add_message("/list: Request user list", MSG_SYSTEM);
        add_message("/msg <user> <message>: Send private message", MSG_SYSTEM);
        add_message("/help: Show this help", MSG_SYSTEM);
        add_message("/quit: Exit the chat", MSG_SYSTEM);
    } else if (strcmp(cmd, "/list") == 0) {
        request_user_list();
    } else if (strcmp(cmd, "/quit") == 0 || strcmp(cmd, "/exit") == 0) {
        send_leave_message();
        running = 0;
    } else if (strncmp(cmd, "/msg ", 5) == 0) {
        char dest[MAX_BUF] = "";
        char content[MAX_BUF] = "";
        
        char *cmd_copy = strdup(cmd + 5); // Skip "/msg "
        
        // Extract destination(s)
        char *space = strchr(cmd_copy, ' ');
        if (space != NULL) {
            *space = '\0';
            strncpy(dest, cmd_copy, MAX_BUF - 1);
            strncpy(content, space + 1, MAX_BUF - 1);
            
            if (strlen(content) > 0) {
                send_message("PRIV", dest, content);
                
                char display[MAX_BUF];
                snprintf(display, MAX_BUF, "To [%s]: %s", dest, content);
                add_message(display, MSG_PRIVATE);
            } else {
                add_message("Message content is empty", MSG_ERROR);
            }
        } else {
            add_message("Invalid format: /msg <user> <message>", MSG_ERROR);
        }
        
        free(cmd_copy);
    } else {
        char error[MAX_BUF];
        snprintf(error, MAX_BUF, "Unknown command: %s", cmd);
        add_message(error, MSG_ERROR);
    }
}

// Select a user (for user selection mode)
void select_user(int direction) {
    pthread_mutex_lock(&user_list_mutex);
    
    selected_user += direction;
    
    if (selected_user < 0) {
        selected_user = 0;
    } else if (selected_user >= user_count) {
        selected_user = user_count - 1;
    }
    
    draw_user_list();
    
    pthread_mutex_unlock(&user_list_mutex);
}

void update_ui() {
    draw_user_list();
    draw_messages();
    draw_input();
    draw_status_bar();
}

void handle_server_message(const char *type, const char *source, const char *dest, const char *content) {
    char display[MAX_BUF];
    
    if (strcmp(type, "JOIN") == 0) {
        add_message(content, MSG_SYSTEM);
    } else if (strcmp(type, "LIST") == 0) {
        if (strcmp(content, "DENIED") == 0) {
            add_message("Server denied your request for user list.", MSG_ERROR);
        } else {
            handle_user_list(content);
            draw_user_list();
        }
    } else if (strcmp(type, "MSG") == 0) {
        snprintf(display, MAX_BUF, "[%s]: %s", source, content);
        add_message(display, MSG_NORMAL);
    } else if (strcmp(type, "PRIV") == 0) {
        if (strcmp(source, username) == 0) {
            snprintf(display, MAX_BUF, "To [%s]: %s", dest, content);
            add_message(display, MSG_PRIVATE);
        } else {
            snprintf(display, MAX_BUF, "From [%s]: %s", source, content);
            add_message(display, MSG_PRIVATE);
        }
    }
}

void process_received_message(const char *message) {
    if (message == NULL || strlen(message) == 0) {
        add_message("Received empty message from server", MSG_ERROR);
        return;
    }
    
    char *msg_copy = strdup(message);
    if (msg_copy == NULL) {
        add_message("Memory allocation error processing message", MSG_ERROR);
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
            handle_server_message(type, source, dest, content);
        } else if (strcmp(type, "MSG") == 0) {
            // Regular broadcast message
            char display_msg[MAX_BUF];
            snprintf(display_msg, sizeof(display_msg), "<%s> %s", source, content ? content : "");
            add_message(display_msg, MSG_NORMAL);
        } else if (strcmp(type, "PRIV") == 0) {
            // Private message
            char display_msg[MAX_BUF];
            if (strcmp(source, username) == 0) {
                snprintf(display_msg, sizeof(display_msg), "To <%s>: %s", dest, content ? content : "");
            } else {
                snprintf(display_msg, sizeof(display_msg), "From <%s>: %s", source, content ? content : "");
            }
            add_message(display_msg, MSG_PRIVATE);
        } else {
            // Unknown message type
            char display_msg[MAX_BUF];
            snprintf(display_msg, sizeof(display_msg), "Unknown message type from %s: %s", source, type);
            add_message(display_msg, MSG_ERROR);
        }
    } else {
        // Invalid message format
        add_message("Invalid message format from server", MSG_ERROR);
    }
    
    free(msg_copy);
    
    // Update UI
    update_ui();
}

void send_message(const char *type, const char *dest, const char *content) {
    char message[MAX_BUF];
    snprintf(message, sizeof(message), "%s|%s|%s|%s", type, username, dest ? dest : "", content ? content : "");
    
    if (server_fifo == -1) {
        add_message("Not connected to server", MSG_ERROR);
        return;
    }
    
    ssize_t bytes_written = write(server_fifo, message, strlen(message));
    if (bytes_written == -1) {
        char error_msg[MAX_BUF];
        snprintf(error_msg, sizeof(error_msg), "Error sending message: %s", strerror(errno));
        add_message(error_msg, MSG_ERROR);
        
        // Try to reconnect
        close(server_fifo);
        server_fifo = open(SERVER_FIFO, O_WRONLY);
        if (server_fifo != -1) {
            add_message("Reconnected to server", MSG_SYSTEM);
            // Retry sending the message
            bytes_written = write(server_fifo, message, strlen(message));
            if (bytes_written == -1) {
                snprintf(error_msg, sizeof(error_msg), "Error sending message after reconnect: %s", strerror(errno));
                add_message(error_msg, MSG_ERROR);
            }
        } else {
            add_message("Failed to reconnect to server", MSG_ERROR);
        }
    }
}

void handle_user_list(const char *list) {
    pthread_mutex_lock(&user_list_mutex);
    
    // Reset current list
    user_count = 0;
    memset(user_list, 0, sizeof(user_list));
    
    add_message("Received user list:", MSG_SYSTEM);
    
    // Parse comma-separated list
    char list_copy[MAX_BUF];
    strncpy(list_copy, list, MAX_BUF-1);
    list_copy[MAX_BUF-1] = '\0';
    
    char *token = strtok(list_copy, ",");
    while (token != NULL && user_count < MAX_USERS) {
        // Skip own username
        if (strcmp(token, username) != 0) {
            strncpy(user_list[user_count], token, 31);
            user_list[user_count][31] = '\0';
            
            char msg[MAX_BUF];
            snprintf(msg, MAX_BUF, "  %d. %s", user_count + 1, user_list[user_count]);
            add_message(msg, MSG_SYSTEM);
            
            user_count++;
        }
        token = strtok(NULL, ",");
    }
    
    pthread_mutex_unlock(&user_list_mutex);
    
    // Update user list display
    draw_user_list();
}

void request_user_list() {
    char message[MAX_BUF];
    snprintf(message, MAX_BUF, "LIST|%s||", username);
    ssize_t bytes = write(server_fifo, message, strlen(message));
    if (bytes <= 0) {
        add_message("Failed to send LIST message", MSG_ERROR);
    } else {
        add_message("Requesting user list...", MSG_SYSTEM);
    }
}

void send_join_message() {
    char message[MAX_BUF];
    snprintf(message, MAX_BUF, "JOIN|%s||", username);
    ssize_t bytes = write(server_fifo, message, strlen(message));
    if (bytes <= 0) {
        add_message("Failed to send JOIN message", MSG_ERROR);
    } else {
        add_message("Joining chat server...", MSG_SYSTEM);
    }
}

void send_leave_message() {
    char message[MAX_BUF];
    snprintf(message, MAX_BUF, "LEAVE|%s||", username);
    write(server_fifo, message, strlen(message));
    add_message("Leaving chat server...", MSG_SYSTEM);
}

void handle_resize(int signo) {
    (void)signo;  // Mark parameter as intentionally unused
    
    pthread_mutex_lock(&resize_mutex);
    resize_required = 1;
    pthread_mutex_unlock(&resize_mutex);
}

void resize_windows() {
    pthread_mutex_lock(&resize_mutex);
    
    if (resize_required) {
        // Get new screen dimensions
        int max_y, max_x;
        endwin();
        refresh();
        getmaxyx(stdscr, max_y, max_x);
        
        // Resize windows
        if (user_list_win) delwin(user_list_win);
        if (message_win) delwin(message_win);
        if (input_win) delwin(input_win);
        
        // Create new windows with updated dimensions
        user_list_win = newwin(max_y - 1, USER_LIST_WIDTH, 0, 0);
        message_win = newwin(max_y - INPUT_HEIGHT - 1, max_x - USER_LIST_WIDTH, 0, USER_LIST_WIDTH);
        input_win = newwin(INPUT_HEIGHT, max_x, max_y - INPUT_HEIGHT, 0);
        
        scrollok(message_win, TRUE);
        
        resize_required = 0;
    }
    
    pthread_mutex_unlock(&resize_mutex);
    
    // Redraw everything
    draw_user_list();
    draw_messages();
    draw_input();
    draw_status_bar();
}

void *receive_messages(void *arg) {
    (void)arg;  // Mark parameter as intentionally unused
    
    char buf[MAX_BUF];
    
    while (running) {
        // Check for resize events
        pthread_mutex_lock(&resize_mutex);
        int need_resize = resize_required;
        pthread_mutex_unlock(&resize_mutex);
        
        if (need_resize) {
            resize_windows();
        }
        
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
                add_message("Lost connection to server", MSG_ERROR);
                if (!running) break;
                sleep(1); // Don't try to reconnect too aggressively
            }
        } else if (errno != EAGAIN) {
            // Error reading from FIFO
            add_message("Error reading from server", MSG_ERROR);
        }
        
        usleep(100000); // Sleep for 100ms to avoid high CPU usage
    }
    
    return NULL;
}

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
    
    // Restore terminal
    cleanup_ui();
}

void signal_handler(int signo) {
    (void)signo; // Mark parameter as unused
    running = 0;
}

// Kullanıcı adı doğrulama fonksiyonu
int validate_username(const char *username) {
    // Check for empty username
    if (username == NULL || strlen(username) == 0) {
        return 0;
    }

    // Check length (3-31 characters)
    size_t len = strlen(username);
    if (len < 3 || len > 31) {
        return 0;
    }

    // First character must be a letter
    if (!isalpha(username[0])) {
        return 0;
    }

    // Only allow alphanumeric characters and underscore
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            return 0;
        }
    }

    return 1;
}

// Kullanıcı adı alma fonksiyonu
void get_username(void) {
    char temp_username[32];
    int valid = 0;

    while (!valid) {
        clear();
        printw("Enter your username (3-31 characters, alphanumeric and underscore only):\n");
        printw("Username must start with a letter.\n> ");
        refresh();

        echo();
        getnstr(temp_username, sizeof(temp_username) - 1);
        noecho();

        if (validate_username(temp_username)) {
            strncpy(username, temp_username, sizeof(username) - 1);
            username[sizeof(username) - 1] = '\0';
            valid = 1;
        } else {
            clear();
            printw("Invalid username! Press any key to try again...\n");
            refresh();
            getch();
        }
    }
}

int connect_to_server(void) {
    // Open server FIFO for writing
    server_fifo = open(SERVER_FIFO, O_WRONLY);
    if (server_fifo == -1) {
        add_message("Cannot connect to server", MSG_ERROR);
        return 0;
    }
    
    // Create client FIFO
    snprintf(client_fifo_name, sizeof(client_fifo_name), CLIENT_FIFO_TEMPLATE, username);
    unlink(client_fifo_name); // Remove if it already exists
    
    if (mkfifo(client_fifo_name, 0666) == -1) {
        add_message("Cannot create client FIFO", MSG_ERROR);
        close(server_fifo);
        server_fifo = -1;
        return 0;
    }
    
    // Open client FIFO for reading
    client_fifo = open(client_fifo_name, O_RDONLY | O_NONBLOCK);
    if (client_fifo == -1) {
        add_message("Cannot open client FIFO for reading", MSG_ERROR);
        close(server_fifo);
        server_fifo = -1;
        unlink(client_fifo_name);
        return 0;
    }
    
    // Send join message to server
    send_join_message();
    
    return 1;
}

int main(void) {
    // Kullanıcı adını al
    get_username();
    
    // Sinyalleri ayarla
    signal(SIGWINCH, handle_resize);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // UI'ı başlat
    init_ui();
    update_ui();
    
    // Server'a bağlan
    if (!connect_to_server()) {
        cleanup_ui();
        exit(1);
    }
    
    // Mesaj alma thread'ini başlat
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        add_message("Failed to create receiver thread", MSG_ERROR);
        cleanup();
        exit(1);
    }
    
    // Ana döngü
    while (running) {
        int ch = getch();
        if (ch != ERR) {
            process_input(ch);
            update_ui();
        }
    }
    
    // Temizlik
    pthread_join(recv_thread, NULL);
    cleanup();
    return 0;
} 