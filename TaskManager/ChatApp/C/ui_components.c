#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include "ui_components.h"

// Global UI state
static ui_state_t ui_state;
static struct termios orig_termios;

// Initialize UI
void ui_init(void) {
    // Get terminal size
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    ui_state.window_height = w.ws_row;
    ui_state.window_width = w.ws_col;
    
    // Calculate layout
    ui_state.chat_end_y = ui_state.window_height - INPUT_HEIGHT - STATUS_BAR_HEIGHT;
    ui_state.input_start_y = ui_state.chat_end_y + 1;
    
    // Initialize state
    ui_state.show_status_bar = true;
    ui_state.show_user_list = true;
    memset(ui_state.status_message, 0, sizeof(ui_state.status_message));
    memset(ui_state.error_message, 0, sizeof(ui_state.error_message));
    memset(ui_state.success_message, 0, sizeof(ui_state.success_message));
    
    // Clear screen and draw initial UI
    ui_clear_screen();
    ui_draw_borders();
}

// Cleanup UI
void ui_cleanup(void) {
    ui_clear_screen();
    printf(COLOR_RESET);
    fflush(stdout);
}

// Clear screen
void ui_clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

// Show status message
void ui_show_status(const char *message) {
    strncpy(ui_state.status_message, message, sizeof(ui_state.status_message) - 1);
    ui_refresh();
}

// Show error message
void ui_show_error(const char *message) {
    strncpy(ui_state.error_message, message, sizeof(ui_state.error_message) - 1);
    ui_refresh();
}

// Show success message
void ui_show_success(const char *message) {
    strncpy(ui_state.success_message, message, sizeof(ui_state.success_message) - 1);
    ui_refresh();
}

// Update chat window with new message
void ui_update_chat_window(const char *message) {
    // Save cursor position
    printf("\033[s");
    
    // Move to chat window area
    printf("\033[%d;%dH", PADDING + 1, PADDING + 1);
    
    // Print message with timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("%02d:%02d:%02d %s\n", t->tm_hour, t->tm_min, t->tm_sec, message);
    
    // Restore cursor position
    printf("\033[u");
    fflush(stdout);
}

// Update user list
void ui_update_user_list(const char *users) {
    if (!ui_state.show_user_list) return;
    
    // Save cursor position
    printf("\033[s");
    
    // Move to user list area
    int start_x = ui_state.window_width - USER_LIST_WIDTH + 1;
    printf("\033[%d;%dH", PADDING + 1, start_x);
    
    // Print user list header
    printf(STYLE_BOLD "Online Users" COLOR_RESET "\n");
    
    // Print users
    char *user_list = strdup(users);
    char *user = strtok(user_list, "\n");
    while (user) {
        printf("\033[%d;%dH%s", PADDING + 2, start_x, user);
        user = strtok(NULL, "\n");
    }
    free(user_list);
    
    // Restore cursor position
    printf("\033[u");
    fflush(stdout);
}

// Show file transfer progress
void ui_show_file_progress(const char *sender, const char *filename, long size) {
    ui_show_status("File transfer started...");
    printf("\n%s is sending %s (%ld bytes)\n", sender, filename, size);
    printf("[                                        ] 0%%");
    fflush(stdout);
}

// Update progress bar
void ui_update_progress(size_t current, size_t total) {
    int percentage = (int)((current * 100) / total);
    int bars = (percentage * 40) / 100;
    
    printf("\r[");
    for (int i = 0; i < 40; i++) {
        if (i < bars) printf("=");
        else printf(" ");
    }
    printf("] %d%%", percentage);
    fflush(stdout);
    
    if (current >= total) {
        printf("\n");
    }
}

// Get user input
bool ui_get_input(char *buffer, size_t size) {
    // Move cursor to input area
    printf("\033[%d;%dH", ui_state.input_start_y, PADDING + 1);
    printf(COLOR_CYAN "> " COLOR_RESET);
    fflush(stdout);
    
    // Read input
    if (fgets(buffer, size, stdin) == NULL) {
        return false;
    }
    
    // Remove newline
    buffer[strcspn(buffer, "\n")] = 0;
    
    // Clear input area
    printf("\033[%d;%dH%*s", ui_state.input_start_y, PADDING + 1, 
           ui_state.window_width - 2, "");
    
    return true;
}

// Get username from user
void ui_get_username(char *buffer, size_t size) {
    printf("Enter your username: ");
    fflush(stdout);
    
    if (fgets(buffer, size, stdin) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;
    }
}

// Draw UI borders
void ui_draw_borders(void) {
    int width = ui_state.window_width;
    int height = ui_state.window_height;
    
    // Draw main box
    printf("%s", BOX_TOP_LEFT);
    for (int i = 1; i < width - 1; i++) printf("%s", BOX_HORIZONTAL);
    printf("%s\n", BOX_TOP_RIGHT);
    
    for (int i = 1; i < height - 1; i++) {
        printf("%s", BOX_VERTICAL);
        for (int j = 1; j < width - 1; j++) printf(" ");
        printf("%s\n", BOX_VERTICAL);
    }
    
    printf("%s", BOX_BOTTOM_LEFT);
    for (int i = 1; i < width - 1; i++) printf("%s", BOX_HORIZONTAL);
    printf("%s\n", BOX_BOTTOM_RIGHT);
    
    // Draw user list separator if enabled
    if (ui_state.show_user_list) {
        int start_x = width - USER_LIST_WIDTH;
        for (int i = 1; i < height - 1; i++) {
            printf("\033[%d;%dH%s", i, start_x, BOX_VERTICAL);
        }
        printf("\033[%d;%dH%s", 0, start_x, BOX_T_DOWN);
        printf("\033[%d;%dH%s", height - 1, start_x, BOX_T_UP);
    }
    
    // Draw input area separator
    int input_y = height - INPUT_HEIGHT - 1;
    printf("\033[%d;%dH%s", input_y, 0, BOX_T_RIGHT);
    for (int i = 1; i < width - 1; i++) printf("%s", BOX_HORIZONTAL);
    printf("%s", BOX_T_LEFT);
    
    fflush(stdout);
}

// Refresh UI
void ui_refresh(void) {
    // Save cursor position
    printf("\033[s");
    
    // Redraw borders
    ui_draw_borders();
    
    // Update status bar if enabled
    if (ui_state.show_status_bar) {
        printf("\033[%d;%dH", ui_state.window_height - 1, PADDING + 1);
        
        if (strlen(ui_state.error_message) > 0) {
            printf(COLOR_RED "%s" COLOR_RESET, ui_state.error_message);
            memset(ui_state.error_message, 0, sizeof(ui_state.error_message));
        }
        else if (strlen(ui_state.success_message) > 0) {
            printf(COLOR_GREEN "%s" COLOR_RESET, ui_state.success_message);
            memset(ui_state.success_message, 0, sizeof(ui_state.success_message));
        }
        else if (strlen(ui_state.status_message) > 0) {
            printf(COLOR_BLUE "%s" COLOR_RESET, ui_state.status_message);
        }
    }
    
    // Restore cursor position
    printf("\033[u");
    fflush(stdout);
}

// Enable raw mode
void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Disable raw mode
void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
} 