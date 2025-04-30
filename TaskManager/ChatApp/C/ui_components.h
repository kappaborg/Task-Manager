#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include <stdio.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>

// UI state structure
typedef struct {
    int window_height;
    int window_width;
    int input_start_y;
    int chat_end_y;
    bool show_status_bar;
    bool show_user_list;
    char status_message[256];
    char error_message[256];
    char success_message[256];
} ui_state_t;

// Function declarations
void ui_init(void);
void ui_cleanup(void);
void ui_clear_screen(void);
void ui_show_status(const char *message);
void ui_show_error(const char *message);
void ui_show_success(const char *message);
void ui_update_chat_window(const char *message);
void ui_update_user_list(const char *users);
void ui_show_file_progress(const char *sender, const char *filename, long size);
void ui_update_progress(size_t current, size_t total);
bool ui_get_input(char *buffer, size_t size);
void ui_get_username(char *buffer, size_t size);
void ui_draw_borders(void);
void ui_refresh(void);

// Terminal handling
void enable_raw_mode(void);
void disable_raw_mode(void);

// Color definitions
#define COLOR_RESET   "\x1b[0m"
#define COLOR_BLACK   "\x1b[30m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_WHITE   "\x1b[37m"

// Style definitions
#define STYLE_BOLD      "\x1b[1m"
#define STYLE_DIM       "\x1b[2m"
#define STYLE_ITALIC    "\x1b[3m"
#define STYLE_UNDERLINE "\x1b[4m"
#define STYLE_BLINK     "\x1b[5m"
#define STYLE_REVERSE   "\x1b[7m"

// Background colors
#define BG_BLACK   "\x1b[40m"
#define BG_RED     "\x1b[41m"
#define BG_GREEN   "\x1b[42m"
#define BG_YELLOW  "\x1b[43m"
#define BG_BLUE    "\x1b[44m"
#define BG_MAGENTA "\x1b[45m"
#define BG_CYAN    "\x1b[46m"
#define BG_WHITE   "\x1b[47m"

// Box drawing characters
#define BOX_HORIZONTAL "─"
#define BOX_VERTICAL "│"
#define BOX_TOP_LEFT "┌"
#define BOX_TOP_RIGHT "┐"
#define BOX_BOTTOM_LEFT "└"
#define BOX_BOTTOM_RIGHT "┘"
#define BOX_T_DOWN "┬"
#define BOX_T_UP "┴"
#define BOX_T_RIGHT "├"
#define BOX_T_LEFT "┤"
#define BOX_CROSS "┼"

// UI layout constants
#define CHAT_WINDOW_MIN_HEIGHT 10
#define USER_LIST_WIDTH 20
#define STATUS_BAR_HEIGHT 2
#define INPUT_HEIGHT 3
#define PADDING 1

#endif // UI_COMPONENTS_H 