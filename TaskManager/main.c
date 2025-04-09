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

void display_menu(void) {
    clear_screen();

    printf("\n========== Process Manager ==========\n");
    printf("1. List all processes\n");
    printf("2. Filter processes by name\n");
    printf("3. Find process by PID\n");
    printf("4. Terminate a process\n");
    printf("5. Change process priority\n");
    printf("6. Show process states information\n");
    printf("7. Display process tree\n");
    printf("8. Show top resource usage\n");
    printf("9. Group operations\n");
    printf("10. Chat App\n");
    printf("11. Task Scheduler\n");
    printf("0. Exit\n");
    printf("====================================\n");
    printf("Enter your choice: ");
}

void handle_group_operations(void) {
    int pattern_type, operation, param = 0;
    char pattern[256];
    
    clear_screen();
    printf("\n========== Group Operations ==========\n");
    printf("Select pattern type:\n");
    printf("1. Process name\n");
    printf("2. Username\n");
    printf("3. Process state\n");
    printf("Enter your choice (1-3): ");
    scanf("%d", &pattern_type);
    getchar(); 
    
    if (pattern_type < 1 || pattern_type > 3) {
        printf("Invalid choice\n");
        return;
    }
    
    printf("Enter pattern to match: ");
    fgets(pattern, sizeof(pattern), stdin);
    pattern[strcspn(pattern, "\n")] = 0; 
    
    printf("\nSelect operation:\n");
    printf("1. Terminate processes\n");
    printf("2. Change priority\n");
    printf("Enter your choice (1-2): ");
    scanf("%d", &operation);
    getchar(); 
    
    if (operation < 1 || operation > 2) {
        printf("Invalid choice\n");
        return;
    }
    
    if (operation == 2) {
        printf("Enter new priority (-20 to 19, lower is higher priority): ");
        scanf("%d", &param);
        getchar(); 
        
        if (param < -20 || param > 19) {
            printf("Invalid priority value\n");
            return;
        }
    }
    
    process_group_operation(pattern, pattern_type, operation, param);
}

void handle_resource_usage(void) {
    int sort_by, count;
    
    clear_screen();
    printf("\n========== Resource Usage ==========\n");
    printf("Sort by:\n");
    printf("1. CPU usage\n");
    printf("2. Memory usage\n");
    printf("Enter your choice (1-2): ");
    scanf("%d", &sort_by);
    getchar();   
    
    if (sort_by < 1 || sort_by > 2) {
        printf("Invalid choice\n");
        return;
    }
    
    printf("Enter number of processes to show: ");
    scanf("%d", &count);
    getchar(); 
    
    if (count <= 0) {
        printf("Invalid count\n");
        return;
    }
    
    show_top_resource_usage(sort_by, count);
}

void handle_process_tree(void) {
    pid_t pid;
    
    clear_screen();
    printf("\n========== Process Tree ==========\n");
    printf("1. Show all processes\n");
    printf("2. Show tree for specific PID\n");
    printf("Enter your choice (1-2): ");
    
    int choice;
    scanf("%d", &choice);
    getchar(); 
    
    if (choice == 1) {
        display_process_tree(0);
    } else if (choice == 2) {
        printf("Enter PID: ");
        scanf("%d", &pid);
        getchar(); 
        
        if (pid <= 0) {
            printf("Invalid PID\n");
            return;
        }
        
        display_process_tree(pid);
    } else {
        printf("Invalid choice\n");
    }
}

void start_chat_application(void) {
    clear_screen();
    printf("\n========== Chat Application ==========\n");
    printf("1. Start Server\n");
    printf("2. Start Client (Terminal UI)\n");
    printf("3. Start Client (Basic)\n");
    printf("0. Back to main menu\n");
    printf("=====================================\n");
    printf("Enter your choice: ");

    int choice;
    scanf("%d", &choice);
    getchar(); 

    pid_t pid;
    char *chat_app_dir = "Chat App/";
    char path[256];

    switch(choice) {
        case 1:
            snprintf(path, sizeof(path), "%s%s", chat_app_dir, "server");
            pid = fork();
            if (pid == 0) {
                execl(path, "server", NULL);
                perror("Failed to start server");
                exit(1);
            }
            printf("Server started with PID: %d\n", pid);
            break;
        case 2:
            snprintf(path, sizeof(path), "%s%s", chat_app_dir, "client_tui");
            pid = fork();
            if (pid == 0) {
                execl(path, "client_tui", NULL);
                perror("Failed to start TUI client");
                exit(1);
            }
            break;
        case 3:
            snprintf(path, sizeof(path), "%s%s", chat_app_dir, "client");
            pid = fork();
            if (pid == 0) {
                execl(path, "client", NULL);
                perror("Failed to start basic client");
                exit(1);
            }
            break;
        case 0:
            return;
        default:
            printf("Invalid option\n");
            sleep(2);
    }
}

void handle_task_scheduler(void) {
    clear_screen();
    printf("\n========== Task Scheduler ==========\n");
    printf("1. List scheduled tasks\n");
    printf("2. Add new task\n");
    printf("3. Remove task\n");
    printf("4. Start scheduler\n");
    printf("5. Stop scheduler\n");
    printf("6. Filter tasks by name\n");
    printf("7. Add demo task (öncelik değiştirme için)\n");
    printf("0. Back to main menu\n");
    printf("===================================\n");
    printf("Enter your choice: ");

    int choice;
    scanf("%d", &choice);
    getchar();

    switch(choice) {
        case 1:
            list_scheduled_tasks();
            break;
        case 2: {
            char command[256];
            int type;
            time_t execution_time;
            int interval = 0;
            struct tm tm = {0};
            
            printf("Enter command to execute: ");
            fgets(command, sizeof(command), stdin);
            command[strcspn(command, "\n")] = 0;
            
            printf("\nSchedule type:\n");
            printf("1. Run once at specific time\n");
            printf("2. Run at intervals\n");
            printf("3. Run daily at specific time\n");
            printf("Enter choice (1-3): ");
            scanf("%d", &type);
            getchar();
            
            switch(type - 1) {
                case ONCE:
                case DAILY:
                    printf("Enter execution time (YYYY MM DD HH MM): ");
                    scanf("%d %d %d %d %d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
                          &tm.tm_hour, &tm.tm_min);
                    tm.tm_year -= 1900;
                    tm.tm_mon -= 1;
                    execution_time = mktime(&tm);
                    break;
                    
                case INTERVAL:
                    printf("Enter interval in seconds: ");
                    scanf("%d", &interval);
                    execution_time = time(NULL) + interval;
                    break;
            }
            
            add_scheduled_task(command, type - 1, execution_time, interval);
            printf("Task added successfully!\n");
            break;
        }
        case 3: {
            int task_index;
            list_scheduled_tasks();
            printf("\nEnter task ID to remove: ");
            scanf("%d", &task_index);
            remove_scheduled_task(task_index - 1);
            printf("Task removed successfully!\n");
            break;
        }
        case 4:
            run_task_scheduler();
            break;
        case 5:
            stop_task_scheduler();
            break;
        case 6: {
            char name[256];
            printf("Enter search term (example: 'echo' veya 'task'): ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            
            filter_tasks_by_name(name);
            break;
        }
        case 7: {
            char name[256];
            printf("Demo görev için kısa bir ad girin: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = 0;
            
            if (strlen(name) == 0) {
                strcpy(name, "PID Demo");  // Default ad
            }
            
            add_demo_task(name);
            printf("\nDemo görev eklendi. Görev listesini görüntülemek için '1' seçeneğini kullanın.\n");
            printf("Sabit bir PID ile çalışan bir süreç görmek için scheduler'ı başlatın (4. seçenek).\n");
            printf("Daha sonra ana menüye dönüp 'Change process priority' ile bu PID'nin önceliğini değiştirebilirsiniz.\n");
            break;
        }
        case 0:
            return;
        default:
            printf("Invalid option\n");
            sleep(2);
    }
    
    printf("\nPress any key to continue...");
    getchar();
}

int interactive_mode(void) {
    enable_raw_mode();
    
    int selected = 1;
    int running = 1;
    
    while (running) {
        clear_screen();
        printf("\n========== Process Manager (Interactive Mode) ==========\n");
        
        
        for (int i = 1; i <= 11; i++) { 
            if (i == selected) {
                printf("> "); 
            } else {
                printf("  ");
            }
            
            switch(i) {
                case 1: printf("List all processes\n"); break;
                case 2: printf("Filter processes by name\n"); break;
                case 3: printf("Find process by PID\n"); break;
                case 4: printf("Terminate a process\n"); break;
                case 5: printf("Change process priority\n"); break;
                case 6: printf("Show process states information\n"); break;
                case 7: printf("Display process tree\n"); break;
                case 8: printf("Show top resource usage\n"); break;
                case 9: printf("Group operations\n"); break;
                case 10: printf("Chat App\n"); break;
                case 11: printf("Task Scheduler\n"); break;
            }
        }
        
        printf("\nUse arrow keys to navigate, Enter to select\n");
        
        char c;
        read(STDIN_FILENO, &c, 1);
        
        if (c == 27) { 
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return -1;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return -1;
            
            // Arrow keys
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': // Up arrow
                        selected = (selected > 1) ? selected - 1 : 11;  
                        break;
                    case 'B': // Down arrow
                        selected = (selected < 11) ? selected + 1 : 1;  
                        break;
                }
            }
        } else if (c == '\r' || c == '\n') { 
            disable_raw_mode();
            
            // Execute selected option
            switch (selected) {
                case 1: 
                    list_all_processes(); 
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                case 2: {
                    char name[256];
                    printf("\nEnter process name to filter: ");
                    fgets(name, sizeof(name), stdin);
                    name[strcspn(name, "\n")] = 0;
                    filter_processes_by_name(name);
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                }
                case 3: {
                    int pid;
                    printf("\nEnter PID: ");
                    scanf("%d", &pid);
                    getchar();
                    find_process_by_pid(pid);
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                }
                case 4: {
                    pid_t pid;
                    printf("\nEnter PID to terminate: ");
                    scanf("%d", &pid);
                    getchar();
                    terminate_process(pid);
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                }
                case 5: {
                    pid_t pid;
                    int priority;
                    printf("\nEnter PID: ");
                    scanf("%d", &pid);
                    getchar();
                    printf("Enter new priority (-20 to 19): ");
                    scanf("%d", &priority);
                    getchar();
                    change_process_priority(pid, priority);
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                }
                case 6:
                    show_process_states_info();
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                case 7:
                    handle_process_tree();
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                case 8:
                    handle_resource_usage();
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                case 9:
                    handle_group_operations();
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                case 10:
                    start_chat_application();
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
                case 11:
                    handle_task_scheduler();
                    printf("\nPress any key to continue...");
                    getchar();
                    break;
            }
            
            enable_raw_mode();
        } else if (c == 'q' || c == 'Q') {  // Quit
            disable_raw_mode();
            return 0;
        }
    }
    
    disable_raw_mode();
    return 0;
}

int main(void) {
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
    
    interactive_mode();

    return 0;
} 
