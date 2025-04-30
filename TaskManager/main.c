#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include "process_manager.h"

#define DEFAULT_PORT 8990
#define MAX_MENU_ITEMS 15
#define KEY_UP 65
#define KEY_DOWN 66
#define KEY_ENTER 10

// Function to handle terminal settings for interactive mode
struct termios orig_termios;

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Function to clear the screen
void clear_screen(void) {
    printf("\033[2J\033[H");  // ANSI escape sequence to clear screen and move cursor to home
    //This is for making our terminal area much more user-friendly. That means after long time of using terminal it will be messy.
}

void display_welcome_screen(void) {
    clear_screen();
    
    // ANSI renk kodları
    printf("\033[1;38;5;208m"); // Turuncu
    
    printf("\n\n");
    printf("                            .--.       .--.                                      \n");
    printf("                           /    \\     /    \\                                     \n");
    printf("                          |      \\-.-/      |                                    \n");
    printf("                           \\                /                                     \n");
    printf("                            `\\   \\___/    /'                                     \n");
    printf("                              `--. .--'                                          \n");
    printf("\033[1;31m");
    printf("         _  __     _    ____  ____       _    ____  _   _ _____ ____      _     \n");
    printf("        | |/ /    / \\  |  _ \\|  _ \\     / \\  / ___|| | | |_   _|  _ \\    / \\    \n");
    printf("        | ' /    / _ \\ | |_) | |_) |   / _ \\ \\___ \\| | | | | | | |_) |  / _ \\   \n");
    printf("\033[1;33m");
    printf("        | . \\   / ___ \\|  __/|  __/   / ___ \\ ___) | |_| | | | |  _ <  / ___ \\  \n");
    printf("        |_|\\_\\ /_/   \\_\\_|   |_|     /_/   \\_\\____/ \\___/  |_| |_| \\_\\/_/   \\_\\ \n");
    printf("                                                                                \n");
    printf("\033[0m");
    
    printf("\033[1;34m"); // Açık mavi
    printf("=============================================================================================\n");
    printf("|     _____                               __  __                                            |\n");
    printf("|    |  __ \\                             |  \\/  |                                           |\n");
    printf("\033[31;1m|    | |__) | __ ___   ___ ___  ___ ___  | \\  / | __ _ _ __   __ _  __ _  ___ _ _           |\n");
    printf("|    |  ___/ '__/ _ \\ / __/ _ \\/ __/ __| | |\\/| |/ _` | '_ \\ / _` |/ _` |/ _ \\ '__|         |\n");
    printf("\033[33;1m|    | |   | | | (_) | (_|  __/\\__ \\__ \\ | |  | | (_| | | | | (_| | (_| |  __/ |            |\n");
    printf("|    |_|   |_|  \\___/ \\___\\___||___/___/ |_|  |_|\\__,_|_| |_|\\__,_|\\__, |\\___|_|            |\n");
    printf("|                                                                   __/ |                   |\n");
    printf("|                                                                  |___/                    |\n");
    printf("=============================================================================================\n");
    printf("\033[1;36m"); // Açık camgöbeği
    printf("            Unix/Linux Process Manager & Task Scheduler - v1.0\n\n");
    printf("\033[1;32m"); // Açık yeşil
    printf("                   Developed by: \033[1;38;5;208mkappasutra\033[0m\n\n");
    printf("\033[0m"); // Renkleri sıfırla
}

// Global variables for menu
int current_selection = 0;

void print_menu(void) {
    clear_screen();
    printf("\nProcess Manager Menu:\n\n");
    
    const char *menu_items[] = {
        "List all processes",
        "Filter processes by name",
        "Find process by PID",
        "Terminate process",
        "Change process priority",
        "Show process states info",
        "Display process tree",
        "Show top resource usage",
        "Schedule task",
        "List scheduled tasks",
        "Remove scheduled task",
        "Add demo task",
        "Filter tasks by name",
        "Start chat server",
        "Connect to chat"
    };
    
    for (int i = 0; i < MAX_MENU_ITEMS; i++) {
        if (i == current_selection) {
            printf("\033[1;32m-> %d. %s\033[0m\n", i + 1, menu_items[i]);
        } else {
            printf("   %d. %s\n", i + 1, menu_items[i]);
        }
    }
    printf("\n   0. Exit\n");
    printf("\nUse arrow keys to navigate, Enter to select\n");
}

int get_menu_choice(void) {
    enable_raw_mode();
    int c;
    while (1) {
        print_menu();
        c = getchar();
        
        if (c == 27) {  // ESC sequence
            if (getchar() == '[') {
                c = getchar();
                switch (c) {
                    case KEY_UP:
                        if (current_selection > 0) {
                            current_selection--;
                        }
                        break;
                    case KEY_DOWN:
                        if (current_selection < MAX_MENU_ITEMS - 1) {
                            current_selection++;
                        }
                        break;
                }
            }
        } else if (c == KEY_ENTER) {
            disable_raw_mode();
            return current_selection + 1;
        } else if (c == '0') {
            disable_raw_mode();
            return 0;
        }
    }
}

void start_chat_server(void) {
    char port_str[10];
    printf("Enter port number (default: %d): ", DEFAULT_PORT);
    if (fgets(port_str, sizeof(port_str), stdin) == NULL) {
        printf("Error reading port number\n");
        return;
    }
    
    // Remove newline
    port_str[strcspn(port_str, "\n")] = 0;
    
    // Use default port if no input
    if (strlen(port_str) == 0) {
        snprintf(port_str, sizeof(port_str), "%d", DEFAULT_PORT);
    }
    
    // Build command
    char command[256];
    snprintf(command, sizeof(command), 
             "cd ChatApp/C && ./bin/chat_server %s", 
             port_str);
    
    // Fork and exec
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
            return;
        }
    else if (pid == 0) {
        // Child process
        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("Failed to start chat server");
        exit(EXIT_FAILURE);
    }
    else {
        printf("Chat server started on port %s (PID: %d)\n", port_str, pid);
        printf("Use option 15 to connect as a client\n");
    }
}

void connect_to_chat(void) {
    char server_ip[16];
    char port_str[10];
    
    printf("Enter server IP (default: 127.0.0.1): ");
    if (fgets(server_ip, sizeof(server_ip), stdin) == NULL) {
        printf("Error reading server IP\n");
        return;
    }
    
    // Remove newline
    server_ip[strcspn(server_ip, "\n")] = 0;
    
    // Use default IP if no input
    if (strlen(server_ip) == 0) {
        strcpy(server_ip, "127.0.0.1");
    }
    
    printf("Enter port number (default: %d): ", DEFAULT_PORT);
    if (fgets(port_str, sizeof(port_str), stdin) == NULL) {
        printf("Error reading port number\n");
            return;
    }
    
    // Remove newline
    port_str[strcspn(port_str, "\n")] = 0;
    
    // Use default port if no input
    if (strlen(port_str) == 0) {
        snprintf(port_str, sizeof(port_str), "%d", DEFAULT_PORT);
    }
    
    // Build command
    char command[256];
    snprintf(command, sizeof(command), 
             "cd ChatApp/C && ./bin/chat_client %s %s", 
             server_ip, port_str);
    
    // Fork and exec
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return;
    }
    else if (pid == 0) {
        // Child process
        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("Failed to start chat client");
        exit(EXIT_FAILURE);
    }
    else {
        // Parent process
        printf("Chat client started (PID: %d)\n", pid);
        printf("Connecting to chat server...\n");
        
        int status;
        waitpid(pid, &status, 0); // Wait for child process to complete
        
        if (WIFEXITED(status)) {
            printf("Chat session ended.\n");
        } else {
            printf("Chat client terminated unexpectedly.\n");
        }
    }
}

int main(void) {
    //Welcome music
    system("afplay welcome.wav");
    init_task_scheduler();
    
    // Check if running with elevated privileges
    uid_t euid = geteuid();
    if (euid != 0) {
        printf("Warning: Some operations (like changing priorities of system processes) \n");
        printf("may require elevated privileges. Consider running with sudo if needed.\n\n");
        printf("Press Enter to continue...");
        getchar();
    }
    
    display_welcome_screen();
    printf("\nPress Enter to continue to main menu...");
    getchar();
    
    int choice;
    char input[256];
    pid_t pid;
    int priority;
    char name[256];
    char command[256];
    
    while (1) {
        choice = get_menu_choice();
        
        if (choice == 0) {
            printf("\nExiting...\n");
            break;
        }
        
        clear_screen();
        
        switch (choice) {
            case 1:
                list_all_processes_with_threads();
                break;
                
            case 2:
                printf("Enter process name to filter: ");
                if (fgets(name, sizeof(name), stdin) != NULL) {
                    name[strcspn(name, "\n")] = 0;
                    filter_processes_by_name(name);
                }
                break;
                
            case 3:
                printf("Enter PID: ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    pid = atoi(input);
                    if (!find_process_by_pid(pid)) {
                        printf("Process not found\n");
                    }
                }
                break;
                
            case 4:
                printf("Enter PID to terminate: ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    pid = atoi(input);
                    terminate_process(pid);
                }
                break;
                
            case 5:
                printf("Enter PID: ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    pid = atoi(input);
                    printf("Enter new priority (-20 to 19): ");
                    if (fgets(input, sizeof(input), stdin) != NULL) {
                        priority = atoi(input);
                        change_process_priority(pid, priority);
                    }
                }
                break;
                
            case 6:
                show_process_states_info();
                break;
                
            case 7:
                printf("Enter root PID (0 for all): ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    pid = atoi(input);
                    display_process_tree(pid);
                }
                break;
                
            case 8:
                printf("Sort by:\n");
                printf("1. CPU Usage\n");
                printf("2. Memory Usage\n");
                printf("Enter choice: ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    int sort_by = atoi(input);
                    printf("Enter number of processes to show: ");
                    if (fgets(input, sizeof(input), stdin) != NULL) {
                        int count = atoi(input);
                        show_top_resource_usage(sort_by, count);
                    }
                }
                break;
                
            case 9:
                printf("Enter command to schedule: ");
                if (fgets(command, sizeof(command), stdin) != NULL) {
                    command[strcspn(command, "\n")] = 0;
                    
                    printf("\nSchedule type:\n");
                    printf("1. Once at specific time\n");
                    printf("2. Repeat at interval\n");
                    printf("3. Daily at specific time\n");
                    printf("Enter choice: ");
                    
                    if (fgets(input, sizeof(input), stdin) != NULL) {
                        int type = atoi(input);
                        time_t execution_time = 0;
                        int interval = 0;
                        
                        switch (type) {
                            case 1: {
                                printf("Enter execution time (YYYY-MM-DD HH:MM): ");
                                char datetime[20];
                                if (fgets(datetime, sizeof(datetime), stdin) != NULL) {
                                    datetime[strcspn(datetime, "\n")] = 0;
                                    
                                    struct tm tm = {0};
                                    if (sscanf(datetime, "%d-%d-%d %d:%d",
                                             &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                                             &tm.tm_hour, &tm.tm_min) == 5) {
                                        tm.tm_year -= 1900;
                                        tm.tm_mon -= 1;
                                        execution_time = mktime(&tm);
                                        add_scheduled_task(command, ONCE, execution_time, 0);
                                    }
                                }
                                break;
                            }
                                
                            case 2:
                                printf("Enter interval in seconds: ");
                                if (fgets(input, sizeof(input), stdin) != NULL) {
                                    interval = atoi(input);
                                    execution_time = time(NULL) + interval;
                                    add_scheduled_task(command, INTERVAL, execution_time, interval);
                                }
                                break;
                                
                            case 3: {
                                printf("Enter daily time (HH:MM): ");
                                char time_str[6];
                                if (fgets(time_str, sizeof(time_str), stdin) != NULL) {
                                    time_str[strcspn(time_str, "\n")] = 0;
                                    
                                    int hour, min;
                                    if (sscanf(time_str, "%d:%d", &hour, &min) == 2) {
                                        time_t now = time(NULL);
                                        struct tm *tm = localtime(&now);
                                        tm->tm_hour = hour;
                                        tm->tm_min = min;
                                        tm->tm_sec = 0;
                                        execution_time = mktime(tm);
                                        
                                        if (execution_time <= now) {
                                            execution_time += 24 * 60 * 60;
                                        }
                                        
                                        add_scheduled_task(command, DAILY, execution_time, 24 * 60 * 60);
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                break;
                
            case 10:
                list_scheduled_tasks();
                break;
                
            case 11:
                printf("Enter task ID to remove: ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    int task_id = atoi(input) - 1;
                    remove_scheduled_task(task_id);
                }
                break;
                
            case 12:
                printf("Enter demo task name: ");
                if (fgets(name, sizeof(name), stdin) != NULL) {
                    name[strcspn(name, "\n")] = 0;
                    add_demo_task(name);
                }
                break;
                
            case 13:
                printf("Enter task name to filter: ");
                if (fgets(name, sizeof(name), stdin) != NULL) {
                    name[strcspn(name, "\n")] = 0;
                    filter_tasks_by_name(name);
                }
                break;
                
            case 14:
                start_chat_server();
                break;
                
            case 15:
                connect_to_chat();
                break;
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    }
    
   stop_task_scheduler();
    return 0;
} 
