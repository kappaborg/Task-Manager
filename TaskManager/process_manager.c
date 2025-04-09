#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include "process_manager.h"

static const ProcessState process_states[] = {
    {'R', "Running - Process is running or runnable (on run queue)"},
    {'S', "Sleeping - Process is interruptible sleep (waiting for event)"},
    {'D', "Uninterruptible Sleep - Process is in uninterruptible sleep (usually I/O)"},
    {'Z', "Zombie - Process has terminated but not reaped by its parent"},
    {'T', "Stopped - Process is stopped (on a signal)"},
    {'t', "Tracing stop - Process is being traced by debugger"},
    {'X', "Dead - Process is dead (should never be seen)"},
    {'I', "Idle - Kernel idle process"},
    {'\0', NULL} // End of array marker
};

#define MAX_SCHEDULED_TASKS 100

static scheduled_task_t tasks[MAX_SCHEDULED_TASKS];
static int task_count = 0;
static int scheduler_running = 0;
static pthread_t scheduler_thread;
static pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;

void* scheduler_thread_function(void* arg) {
    (void)arg;  // Suppress unused parameter warning
    
    while (scheduler_running) {
        time_t current_time = time(NULL);
        
        pthread_mutex_lock(&task_mutex);
        for (int i = 0; i < task_count; i++) {
            if (!tasks[i].is_active) continue;
            
            int should_run = 0;
            switch (tasks[i].type) {
                case ONCE:
                    if (current_time >= tasks[i].execution_time) {
                        should_run = 1;
                        tasks[i].is_active = 0;  // Bir kerelik görev tamamlandı
                    }
                    break;
                    
                case INTERVAL:
                    if (current_time >= tasks[i].execution_time) {
                        should_run = 1;
                        tasks[i].execution_time = current_time + tasks[i].interval_seconds;
                    }
                    break;
                    
                case DAILY: {
                    struct tm* tm_current = localtime(&current_time);
                    struct tm* tm_scheduled = localtime(&tasks[i].execution_time);
                    if (tm_current->tm_hour == tm_scheduled->tm_hour &&
                        tm_current->tm_min == tm_scheduled->tm_min) {
                        should_run = 1;
                        //Schedule for tomorrow same time 
                        tasks[i].execution_time += 24 * 60 * 60;
                    }
                    break;
                }
            }
            
            if (should_run) {
                // Create a modified command with task identifier
                char task_cmd[512];
                
                // For better identification in process listings
                snprintf(task_cmd, sizeof(task_cmd), 
                        "echo \"Running Task %d: %s\"; %s", 
                        i + 1, tasks[i].command, tasks[i].command);
                
                pid_t pid = fork();
                if (pid == 0) {
                    // Create identifiable process name using Task-ID prefix
                    char process_name[256];
                    snprintf(process_name, sizeof(process_name), "Task-%d", i + 1);
                    
                    // Write the name to /proc/self/comm if available (Linux)
                    FILE *comm_file = fopen("/proc/self/comm", "w");
                    if (comm_file) {
                        fputs(process_name, comm_file);
                        fclose(comm_file);
                    }
                    
                    // Execute the task
                    execl("/bin/sh", "sh", "-c", task_cmd, NULL);
                    exit(1);
                } else if (pid > 0) {
                    tasks[i].last_pid = pid;
                }
            }
        }
        pthread_mutex_unlock(&task_mutex);
        
        sleep(1); 
    }
    return NULL;
}

void init_task_scheduler(void) {
    task_count = 0;
    scheduler_running = 0;
    memset(tasks, 0, sizeof(tasks));
}

void add_scheduled_task(const char* command, schedule_type_t type, time_t execution_time, int interval_seconds) {
    pthread_mutex_lock(&task_mutex);
    
    if (task_count >= MAX_SCHEDULED_TASKS) {
        printf("Maximum task limit reached!\n");
        pthread_mutex_unlock(&task_mutex);
        return;
    }
    
    scheduled_task_t* task = &tasks[task_count];
    strncpy(task->command, command, sizeof(task->command) - 1);
    task->type = type;
    task->execution_time = execution_time;
    task->interval_seconds = interval_seconds;
    task->is_active = 1;
    task->last_pid = 0;
    
    task_count++;
    pthread_mutex_unlock(&task_mutex);
}

void add_demo_task(const char* name) {
    pthread_mutex_lock(&task_mutex);
    
    if (task_count >= MAX_SCHEDULED_TASKS) {
        printf("Maximum task limit reached!\n");
        pthread_mutex_unlock(&task_mutex);
        return;
    }
    
    // Mevcut demo görevlerini kontrol et
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].is_demo_task) {
            printf("Zaten bir demo görevi mevcut (Task ID: %d, PID: %d)\n", i+1, tasks[i].last_pid);
            pthread_mutex_unlock(&task_mutex);
            return;
        }
    }
    
    scheduled_task_t* task = &tasks[task_count];
    
    // Demo görevi adı
    char task_name[256];
    snprintf(task_name, sizeof(task_name), "Demo görev: %s", name);
    strncpy(task->command, task_name, sizeof(task->command) - 1);
    
    task->type = INTERVAL;
    task->execution_time = time(NULL); // Hemen başlat
    task->interval_seconds = 3600; // 1 saat (aslında sürekli çalışacak)
    task->is_active = 1;
    task->last_pid = 0;
    task->is_demo_task = 1; // Bu bir demo görevi
    
    int task_index = task_count; // Görev indeksi için kopyala
    task_count++;
    
    printf("Demo görevi eklendi. Scheduler'ı başlattığınızda, sabit bir PID ile uzun süre çalışacak.\n");
    
    // Eğer scheduler zaten çalışıyorsa, hemen bu görev için bir PID ata
    if (scheduler_running) {
        // Demo görev kontrolcüsünü oluştur
        char task_id[64];
        snprintf(task_id, sizeof(task_id), "Demo-%d-Task", task_index + 1);
        
        // Demo görevi için uzun süre çalışan bir komut
        char command[512];
        
        // Demo görev kontrolcüsü için komut
        snprintf(command, sizeof(command), 
               "export PS1=\"Demo-%d> \"; echo \"[%s] DEMO TASK RUNNING ($(date))\" > /dev/stderr; "
               "echo \"Bu bir demo görevidir. PID:$$. 'Ctrl+C' ile durdurmayın.\"; "
               "echo \"Bu süreç, öncelik değiştirme demosu için kullanılmaktadır.\"; "
               "while true; do echo -n .; sleep 10; done", 
               task_index + 1, task_id);
        
        // Süreci fork() ile oluştur
        pid_t pid = fork();
        if (pid == 0) {
            // Çocuk süreç - demo süreci
            setsid(); // Kendi süreç grubunu oluştur
            
            // Komutun çalıştırılması
            execl("/bin/sh", task_id, "-c", command, NULL);
            exit(0);
        } else if (pid > 0) {
            // Ebeveyn süreç - PID'yi kaydet
            task->last_pid = pid;
            char timestamp[32];
            time_t now = time(NULL);
            strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));
            printf("Demo task started with PID: %d at %s\n", pid, timestamp);
            printf("Bu PID'yi öncelik değiştirme demosu için kullanabilirsiniz.\n");
        }
    } else {
        printf("Bu görev, öncelik değiştirme işlemleri için idealdir.\n");
    }
    
    pthread_mutex_unlock(&task_mutex);
}

void list_scheduled_tasks(void) {
    pthread_mutex_lock(&task_mutex);
    printf("\nScheduled Tasks:\n");
    printf("%-4s %-40s %-10s %-20s %-10s %-10s %-10s\n", 
           "ID", "Command", "Type", "Next Run", "Interval", "Status", "Last PID");
    printf("------------------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < task_count; i++) {
        char type_str[10];
        switch (tasks[i].type) {
            case ONCE: strcpy(type_str, "Once"); break;
            case INTERVAL: strcpy(type_str, "Interval"); break;
            case DAILY: strcpy(type_str, "Daily"); break;
        }
        
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                localtime(&tasks[i].execution_time));
        
        char pid_str[10] = "N/A";
        if (tasks[i].last_pid > 0) {
            snprintf(pid_str, sizeof(pid_str), "%d", tasks[i].last_pid);
        }
        
        printf("%-4d %-40s %-10s %-20s %-10d %-10s %-10s\n", 
               i + 1,
               tasks[i].command,
               type_str,
               time_str,
               tasks[i].interval_seconds,
               tasks[i].is_active ? "Active" : "Inactive",
               pid_str);
    }
    pthread_mutex_unlock(&task_mutex);
}

void remove_scheduled_task(int task_index) {
    pthread_mutex_lock(&task_mutex);
    
    if (task_index < 0 || task_index >= task_count) {
        printf("Invalid task index!\n");
        pthread_mutex_unlock(&task_mutex);
        return;
    }
    
    // Son task'ı silinen yere taşı
    if (task_index < task_count - 1) {
        memcpy(&tasks[task_index], &tasks[task_count - 1], sizeof(scheduled_task_t));
    }
    task_count--;
    
    pthread_mutex_unlock(&task_mutex);
}

void run_task_scheduler(void) {
    if (!scheduler_running) {
        scheduler_running = 1;
        pthread_create(&scheduler_thread, NULL, scheduler_thread_function, NULL);
        printf("Task scheduler started.\n");
    }
}

void stop_task_scheduler(void) {
    if (scheduler_running) {
        scheduler_running = 0;
        pthread_join(scheduler_thread, NULL);
        printf("Task scheduler stopped.\n");
    }
}

void list_all_processes(void) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        return;
    } else if (pid == 0) {
        // Child process
        // macOS için daha detaylı çıktı
        execlp("ps", "ps", "aux", NULL);
        perror("Failed to execute ps command");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            return;
        }
        
        if (!WIFEXITED(status)) {
            printf("Process terminated abnormally\n");
        } else if (WEXITSTATUS(status) != 0) {
            printf("Command failed with status: %d\n", WEXITSTATUS(status));
        }
    }
}

void filter_processes_by_name(const char *name) {
    if (name == NULL || strlen(name) == 0) {
        printf("Invalid process name\n");
        return;
    }
    
    char safe_name[256];
    size_t j = 0;
    for (size_t i = 0; i < strlen(name) && j < sizeof(safe_name) - 1; i++) {
        if (isalnum(name[i]) || name[i] == '_' || name[i] == '-') {
            safe_name[j++] = name[i];
        }
    }
    safe_name[j] = '\0';
    
    if (strlen(safe_name) == 0) {
        printf("Invalid process name after sanitization\n");
        return;
    }
    
    char command[512];
    snprintf(command, sizeof(command), "ps aux | grep -i \"%s\" | grep -v grep", safe_name);
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return;
    } else if (pid == 0) {
        execlp("sh", "sh", "-c", command, NULL);
        perror("Failed to execute command");
        exit(EXIT_FAILURE);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            return;
        }
        
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 1) {
                printf("No processes found matching '%s'\n", safe_name);
            } else if (WEXITSTATUS(status) != 0) {
                printf("Command failed with status: %d\n", WEXITSTATUS(status));
            }
        } else {
            printf("Process terminated abnormally\n");
        }
    }
}

int find_process_by_pid(pid_t target_pid) {
    if (target_pid <= 0) {
        printf("Invalid PID\n");
        return 0;
    }
    
    char command[512];
    snprintf(command, sizeof(command), "ps -p %d -o pid,ppid,user,%%cpu,%%mem,state,command", target_pid);
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return 0;
    } else if (pid == 0) {
        execlp("sh", "sh", "-c", command, NULL);
        perror("Failed to execute command");
        exit(EXIT_FAILURE);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            return 0;
        }
        
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                printf("Process with PID %d not found\n", target_pid);
                return 0;
            }
        } else {
            printf("Command terminated abnormally\n");
            return 0;
        }
    }
    return 1;
}

int terminate_process(pid_t target_pid) {
    if (target_pid <= 0) {
        printf("Invalid PID\n");
        return 0;
    }
    
    if (!find_process_by_pid(target_pid)) {
        return 0;
    }
    
    if (kill(target_pid, SIGTERM) == 0) {
        printf("SIGTERM signal sent to process %d\n", target_pid);
        
        for (int i = 0; i < 5; i++) {
            if (kill(target_pid, 0) != 0) {
                printf("Process %d terminated successfully\n", target_pid);
                return 1;
            }
            sleep(1);
        }
        
        printf("Process didn't terminate with SIGTERM, trying SIGKILL...\n");
        if (kill(target_pid, SIGKILL) == 0) {
            printf("SIGKILL signal sent to process %d\n", target_pid);
            return 1;
        }
    }
    
    if (errno == EPERM) {
        printf("Permission denied to terminate process %d\n", target_pid);
    } else {
        perror("Failed to terminate process");
    }
    return 0;
}

int change_process_priority(pid_t target_pid, int priority) {
    if (target_pid <= 0) {
        printf("Invalid PID\n");
        return 0;
    }
    
    if (priority < -20 || priority > 19) {
        printf("Invalid priority value. Must be between -20 and 19\n");
        return 0;
    }
    
    if (!find_process_by_pid(target_pid)) {
        return 0;
    }
    
    char command[512];
    
    // Check if we need elevated privileges by checking who owns the process
    char owner_check_cmd[256];
    snprintf(owner_check_cmd, sizeof(owner_check_cmd), "ps -o user= -p %d", target_pid);
    
    FILE *fp = popen(owner_check_cmd, "r");
    if (fp == NULL) {
        perror("Failed to check process owner");
        return 0;
    }
    
    char owner[64] = {0};
    if (fgets(owner, sizeof(owner), fp) == NULL) {
        pclose(fp);
        printf("Could not determine process owner\n");
        return 0;
    }
    pclose(fp);
    
    // Trim trailing whitespace
    char *end = owner + strlen(owner) - 1;
    while (end > owner && isspace(*end)) {
        *end = '\0';
        end--;
    }
    
    // Get current user
    char current_user[64] = {0};
    char *user_env = getenv("USER");
    if (user_env) {
        strncpy(current_user, user_env, sizeof(current_user) - 1);
    } else {
        strcpy(current_user, "unknown");
    }
    
    // If process owner is not current user, try with sudo
    if (strcmp(owner, current_user) != 0) {
        snprintf(command, sizeof(command), "sudo renice %d -p %d", priority, target_pid);
        printf("Process is owned by %s. Attempting with sudo...\n", owner);
    } else {
        snprintf(command, sizeof(command), "renice %d -p %d", priority, target_pid);
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return 0;
    } else if (pid == 0) {
        execlp("sh", "sh", "-c", command, NULL);
        perror("Failed to execute renice command");
        exit(EXIT_FAILURE);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            return 0;
        }
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Successfully changed priority of process %d to %d\n", target_pid, priority);
            return 1;
        } else {
            if (errno == EPERM) {
                printf("Permission denied to change process priority\n");
                printf("Try running the program with sudo for system processes\n");
            } else {
                printf("Failed to change process priority\n");
            }
            return 0;
        }
    }
}

void show_process_states_info(void) {
    printf("\n===== Process State Codes and Descriptions PROUDLY DESIGNED BY KAPPASUTRA =====\n");
    printf("%-6s %-70s\n", "CODE", "DESCRIPTION");
    printf("-------------------------------------------------------------------------------------------\n");
    
    int i = 0;
    while (process_states[i].description != NULL) {
        printf("%-6c %-70s\n", process_states[i].code, process_states[i].description);
        i++;
    }
    printf("\nThe process state is shown in the 'STAT' column of ps output.\n");
    printf("Additional characters may appear after the state code:\n");
    printf("  + : The process is in the foreground process group\n");
    printf("  s : The process is a session leader\n");
    printf("  l : The process is multi-threaded\n");
    printf("  < : The process has raised priority\n");
    printf("  N : The process has reduced priority\n");
    printf("-------------------------------------------------------------------------------------------\n");
}

void explain_process_state(char state) {
    int i = 0;
    while (process_states[i].description != NULL) {
        if (process_states[i].code == state) {
            printf("State '%c': %s\n", state, process_states[i].description);
            return;
        }
        i++;
    }
    printf("Unknown process state code: %c\n", state);
}

void display_process_tree(pid_t root_pid) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        return;
    } else if (pid == 0) {
        // Child process
        if (root_pid > 0) {
            // Show tree for specific process
            char pid_str[16];
            snprintf(pid_str, sizeof(pid_str), "%d", root_pid);
            
            // Check if pstree is available
            int has_pstree = (access("/usr/bin/pstree", X_OK) == 0);
            
            if (has_pstree) {
                // Use pstree command if available
                execlp("pstree", "pstree", "-p", pid_str, NULL);
                // If we get here, pstree execution failed
                perror("Failed to execute pstree command");
            }
            
            // Fallback: use ps with filtering for the specific PID
            char command[256];
            snprintf(command, sizeof(command), 
                     "ps -ax -o pid,ppid,command | grep -v grep | awk 'BEGIN {printf \"%%8s %%8s %%s\\n\", \"PID\", \"PPID\", \"COMMAND\"} {print}' | grep -E \"^ *%s|^ *[0-9]+ +%s\"", 
                     pid_str, pid_str);
            execlp("sh", "sh", "-c", command, NULL);
            
            perror("Failed to execute process tree command");
            exit(EXIT_FAILURE);
        } else {
            // Show all processes in tree format
            // Check if pstree is available
            int has_pstree = (access("/usr/bin/pstree", X_OK) == 0);
            
            if (has_pstree) {
                execlp("pstree", "pstree", "-p", NULL);
                // If we get here, pstree execution failed
                perror("Failed to execute pstree command");
            }
            
            // Fallback: use ps with sorting
            char command[256] = "ps -ax -o pid,ppid,command | grep -v grep | sort -nk2";
            execlp("sh", "sh", "-c", command, NULL);
            
            perror("Failed to execute process tree command");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process waits for child
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            printf("Command execution failed with status: %d\n", WEXITSTATUS(status));
        }
    }
}

void show_top_resource_usage(int sort_by, int count) {
    if (count <= 0) {
        count = 10; // Default to top 10 if you want you can change it but believe me after 10 in terminal you will see a lot of processes. So hard to see.
    }
    
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d", count);
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        return;
    } else if (pid == 0) {
        // Child process
        char command[200];
        
        if (sort_by == 1) {
            // Sort by CPU usage
            printf("\n===== Top %d Processes by CPU Usage =====\n", count);
            /*
            // For Windows
            snprintf(command, sizeof(command), "wmic process get ProcessId,Name,Priority,ThreadCount,WorkingSetSize /format:table | sort /R");
            */
            snprintf(command, sizeof(command), "ps -ax -o pid,%%cpu,%%mem,command -r | head -%s", count_str);
        } else {
            // Sort by memory usage
            printf("\n===== Top %d Processes by Memory Usage =====\n", count);
            /*
            // For Windows
            snprintf(command, sizeof(command), "wmic process get ProcessId,Name,WorkingSetSize /format:table | sort /R");
            */
            snprintf(command, sizeof(command), "ps -ax -o pid,%%mem,%%cpu,rss,command -m | head -%s", count_str);
        }
        
        execlp("sh", "sh", "-c", command, NULL);
        
        perror("Failed to execute top resource command");
        exit(EXIT_FAILURE);
    } else {
        // Parent process waits for child
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            printf("Command execution failed with status: %d\n", WEXITSTATUS(status));
        }
    }
}

int get_process_info(pid_t target_pid, ProcessInfo *info) {
    if (target_pid <= 0 || info == NULL) {
        return 0;
    }
    
    // Initialize ProcessInfo structure
    info->pid = target_pid;
    info->ppid = 0;
    info->state = '?';
    info->username = NULL;
    info->command = NULL;
    info->cpu_percent = 0.0;
    info->mem_percent = 0.0;
    info->memory_kb = 0;
    info->start_time = NULL;
    
    // Create temporary file to store ps output
    char tmpfile[] = "/tmp/procinfo.XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd == -1) {
        perror("Failed to create temporary file");
        return 0;
    }
    close(fd);
    
    // Get process info using ps command
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        unlink(tmpfile);
        return 0;
    } else if (pid == 0) {
        // Child process
        // Redirect output to temp file
        freopen(tmpfile, "w", stdout);
        
        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%d", target_pid);
        
        // Use detailed ps format for parsing
        /*
        // For Windows
        char command[256];
        snprintf(command, sizeof(command), "wmic process where ProcessId=%s get ProcessId,ParentProcessId,Name,Priority,WorkingSetSize /format:list", pid_str);
        execlp("cmd", "cmd", "/c", command, NULL);
        */
        char *args[] = {"ps", "-p", pid_str, "-o", "pid,ppid,user,state,%cpu,%mem,rss,lstart,command", NULL};
        execvp("ps", args);
        
        perror("Failed to execute ps command");
        exit(EXIT_FAILURE);
    } else {
        // Parent process waits for child
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                unlink(tmpfile);
                return 0; // Process not found
            }
            
            // Process exists, parse the info from temp file
            FILE *fp = fopen(tmpfile, "r");
            if (fp == NULL) {
                perror("Failed to open temp file");
                unlink(tmpfile);
                return 0;
            }
            
            char line[1024];
            // Skip header line
            if (fgets(line, sizeof(line), fp) == NULL) {
                fclose(fp);
                unlink(tmpfile);
                return 0;
            }
            
            // Read process info line
            if (fgets(line, sizeof(line), fp) != NULL) {
                // Parse fields - this is a simplified version and might need refinement
                char *token = strtok(line, " \t");
                if (token) info->pid = atoi(token);
                
                token = strtok(NULL, " \t");
                if (token) info->ppid = atoi(token);
                
                token = strtok(NULL, " \t");
                if (token) {
                    info->username = strdup(token);
                }
                
                token = strtok(NULL, " \t");
                if (token) info->state = token[0];
                
                token = strtok(NULL, " \t");
                if (token) info->cpu_percent = atof(token);
                
                token = strtok(NULL, " \t");
                if (token) info->mem_percent = atof(token);
                
                token = strtok(NULL, " \t");
                if (token) info->memory_kb = atol(token);
                
                // Remaining tokens are start time and command
                // Getting start time properly is tricky, simplified here
                token = strtok(NULL, "\n");
                if (token) {
                    char *command_start = strstr(token, "/");
                    if (command_start) {
                        *command_start = '\0';
                        info->start_time = strdup(token);
                        info->command = strdup(command_start + 1);
                    } else {
                        info->start_time = strdup("Unknown");
                        info->command = strdup(token);
                    }
                }
            }
            
            fclose(fp);
            unlink(tmpfile);
            return 1;
        }
    }
    
    unlink(tmpfile);
    return 0;
}

void free_process_info(ProcessInfo *info) {
    if (info == NULL) return;
    
    if (info->username) free(info->username);
    if (info->command) free(info->command);
    if (info->start_time) free(info->start_time);
    
    // Reset to safe defaults
    info->username = NULL;
    info->command = NULL;
    info->start_time = NULL;
}

int process_group_operation(const char *pattern, int pattern_type, int operation, int param) {
    if (pattern == NULL || strlen(pattern) == 0) {
        printf("Invalid pattern\n");
        return 0;
    }
    
    // Create temporary file to store process list
    char tmpfile[] = "/tmp/procgroup.XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd == -1) {
        perror("Failed to create temporary file");
        return 0;
    }
    close(fd);
    
    // Get list of processes matching the pattern
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        unlink(tmpfile);
        return 0;
    } else if (pid == 0) {
        // Child process
        // Redirect output to temp file
        freopen(tmpfile, "w", stdout);
        
        char command[512];
        switch (pattern_type) {
            case 1: // Filter by name
                /*
                For Windows
                snprintf(command, sizeof(command), "wmic process where \"Name like '%%%s%%'\" get ProcessId /format:list | findstr /R \"ProcessId=\"", pattern);
                */
                snprintf(command, sizeof(command), "ps -ax | grep -i \"%s\" | grep -v grep | awk '{print $1}'", pattern);
                break;
            case 2: // Filter by user
                /*
                // For Windows - this is challenging, may need more complex command
                // WMIC doesn't have a direct way to filter by username
                */
                snprintf(command, sizeof(command), "ps -ax -o pid,user | grep -i \"%s\" | grep -v grep | awk '{print $1}'", pattern);
                break;
            case 3: // Filter by state
                /*
                // For Windows - unfortunatelly no direct equivalent
                */
                snprintf(command, sizeof(command), "ps -ax -o pid,state | grep -i \"[[:space:]]%s\" | awk '{print $1}'", pattern);
                break;
            default:
                fprintf(stderr, "Invalid pattern type\n");
                exit(EXIT_FAILURE);
        }
        
        execlp("sh", "sh", "-c", command, NULL);
        
        perror("Failed to execute process filtering command");
        exit(EXIT_FAILURE);
    } else {
        // Parent process waits for child
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            printf("Process filtering failed with status: %d\n", WEXITSTATUS(status));
            unlink(tmpfile);
            return 0;
        }
        
        // Read PIDs from temp file and perform operation
        FILE *fp = fopen(tmpfile, "r");
        if (fp == NULL) {
            perror("Failed to open temp file");
            unlink(tmpfile);
            return 0;
        }
        
        pid_t target_pid;
        int count = 0;
        int success_count = 0;
        
        printf("\nProcesses matching the pattern:\n");
        
        // First pass: display matching processes
        while (fscanf(fp, "%d", &target_pid) == 1) {
            count++;
            
            // Display process info
            ProcessInfo info;
            if (get_process_info(target_pid, &info)) {
                printf("[%d] PID: %d, User: %s, Command: %s\n", 
                       count, info.pid, info.username, info.command);
                free_process_info(&info);
            } else {
                printf("[%d] PID: %d (Unable to get details)\n", count, target_pid);
            }
        }
        
        if (count == 0) {
            printf("No processes found matching the pattern\n");
            fclose(fp);
            unlink(tmpfile);
            return 0;
        }
        
        // Ask for confirmation
        printf("\nFound %d processes. Proceed with ", count);
        if (operation == 1) {
            printf("termination");
        } else if (operation == 2) {
            printf("priority change to %d", param);
        }
        printf("? (y/n): ");
        
        char response;
        scanf("%c", &response);
        getchar(); // Consume newline
        
        if (response != 'y' && response != 'Y') {
            printf("Operation cancelled\n");
            fclose(fp);
            unlink(tmpfile);
            return 0;
        }
        
        // Second pass: perform the operation
        rewind(fp);
        while (fscanf(fp, "%d", &target_pid) == 1) {
            if (operation == 1) {
                // Terminate process
                if (terminate_process(target_pid)) {
                    success_count++;
                }
            } else if (operation == 2) {
                // Change priority
                if (change_process_priority(target_pid, param)) {
                    success_count++;
                }
            }
        }
        
        fclose(fp);
        unlink(tmpfile);
        
        printf("\nOperation completed on %d/%d processes\n", success_count, count);
        return success_count;
    }
    
    unlink(tmpfile);
    return 0;
}

void filter_tasks_by_name(const char *name) {
    if (name == NULL || *name == '\0') {
        printf("Invalid filter name\n");
        return;
    }
    
    pthread_mutex_lock(&task_mutex);
    printf("\nTasks matching '%s':\n", name);
    printf("%-4s %-40s %-10s %-10s\n", 
           "ID", "Command", "Status", "PID");
    printf("-------------------------------------------------------------\n");
    
    int found = 0;
    for (int i = 0; i < task_count; i++) {
        if (strstr(tasks[i].command, name) != NULL) {
            char pid_str[10] = "N/A";
            if (tasks[i].last_pid > 0) {
                // Check if process is still running
                char check_cmd[64];
                snprintf(check_cmd, sizeof(check_cmd), "ps -p %d -o pid= > /dev/null 2>&1", 
                         tasks[i].last_pid);
                if (system(check_cmd) == 0) {
                    snprintf(pid_str, sizeof(pid_str), "%d", tasks[i].last_pid);
                } else {
                    strcpy(pid_str, "Ended");
                }
            }
            
            printf("%-4d %-40s %-10s %-10s\n", 
                   i + 1,
                   tasks[i].command,
                   tasks[i].is_active ? "Active" : "Inactive",
                   pid_str);
            found++;
        }
    }
    
    if (!found) {
        printf("No tasks found matching '%s'\n", name);
    }
    pthread_mutex_unlock(&task_mutex);
} 
