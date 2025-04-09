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
#include <pwd.h>
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

// Signal handler to reap zombie processes
static void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void* scheduler_thread_function(void* arg) {
    (void)arg;  // Suppress unused parameter warning
    
    // Set up signal handler to reap zombie processes
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror("sigaction");
        return NULL;
    }
    
    while (scheduler_running) {
        time_t current_time = time(NULL);
        
        pthread_mutex_lock(&task_mutex);
        for (int i = 0; i < task_count; i++) {
            if (!tasks[i].is_active) continue;
            
            // Demo görev kontrolü - eğer aktif uzun süreli bir görev varsa çalıştırmayı atla
            if (tasks[i].is_demo_task && tasks[i].last_pid > 0) {
                // Check if process still exists
                char check_cmd[128];
                snprintf(check_cmd, sizeof(check_cmd), "ps -p %d > /dev/null 2>&1", tasks[i].last_pid);
                if (system(check_cmd) == 0) {
                    // Process still running, skip
                    pthread_mutex_unlock(&task_mutex);
                    continue;
                }
                // Process ended, allow restart
            }
            
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
                        
                        // Interval çok kısa ise (demo için)
                        if (tasks[i].interval_seconds <= 5) {
                            // Çalışan süreç var mı kontrol et
                            if (tasks[i].last_pid > 0) {
                                char check_cmd[128];
                                snprintf(check_cmd, sizeof(check_cmd), "ps -p %d > /dev/null 2>&1", tasks[i].last_pid);
                                if (system(check_cmd) == 0) {
                                    // Süreç hala çalışıyor, yeniden başlatmayı iptal et
                                    should_run = 0;
                                }
                            }
                        }
                    }
                    break;
                    
                case DAILY: {
                    struct tm* tm_current = localtime(&current_time);
                    struct tm* tm_scheduled = localtime(&tasks[i].execution_time);
                    
                    // Tam tarih kontrolü - eğer execution_time, current_time'dan küçük veya eşitse
                    // VE aynı gün içindeyse çalıştır
                    if (tasks[i].execution_time <= current_time) {
                        // Aynı gün içinde olup olmadığını kontrol et (yıl, ay, gün)
                        if (tm_current->tm_year == tm_scheduled->tm_year &&
                            tm_current->tm_mon == tm_scheduled->tm_mon &&
                            tm_current->tm_mday == tm_scheduled->tm_mday &&
                            tm_current->tm_hour == tm_scheduled->tm_hour &&
                            tm_current->tm_min == tm_scheduled->tm_min) {
                            
                            should_run = 1;
                            // Bir sonraki gün için yeniden zamanla
                            tasks[i].execution_time += 24 * 60 * 60;
                        }
                    }
                    break;
                }
            }
            
            if (should_run) {
                // Eğer hâlihazırda bir PID varsa, bu bir kontrol süreci olabilir, sonlandır
                if (tasks[i].last_pid > 0) {
                    // Mevcut süreci kontrol et
                    char check_cmd[128];
                    snprintf(check_cmd, sizeof(check_cmd), "ps -p %d > /dev/null 2>&1", 
                             tasks[i].last_pid);
                    
                    if (system(check_cmd) == 0) {
                        // Demo görev ise, çalışmaya devam et, yeniden başlatma
                        if (tasks[i].is_demo_task) {
                            pthread_mutex_unlock(&task_mutex);
                            continue;
                        }
                        
                        // Süreç hâlâ çalışıyor, sonlandır
                        kill(tasks[i].last_pid, SIGTERM);
                        // Kısa bir süre bekle
                        usleep(100000); // 100ms
                    }
                }
                
                // Create a task with identifiable name
                char task_cmd[512];
                
                // Özel Task identifier (pid + task id)
                char task_id[64];
                snprintf(task_id, sizeof(task_id), "TASKID-%d", i + 1);
                
                // Demo görevi ise uzun süre çalışan bir komut ekle
                if (tasks[i].is_demo_task) {
                    // Demo görevi için özel işlemler - bekleyecek süreç
                    snprintf(task_cmd, sizeof(task_cmd), 
                           "export PS1=\"Task-%d> \"; echo \"[%s] DEMO TASK RUNNING ($(date))\" > /dev/stderr; "
                           "echo \"Bu bir demo görevidir. PID:$$. 'Ctrl+C' ile durdurmayın.\"; "
                           "echo \"Bu süreç, öncelik değiştirme demosu için kullanılmaktadır.\"; "
                           "while true; do echo -n .; sleep 10; done", 
                           i + 1, task_id);
                } else {
                    // Normal görev
                    snprintf(task_cmd, sizeof(task_cmd), 
                           "export PS1=\"Task-%d> \"; echo \"[%s] Executing: %s ($(date))\"; %s; echo \"[%s] Completed: %s ($(date))\"", 
                           i + 1, task_id, tasks[i].command, tasks[i].command, task_id, tasks[i].command);
                }
                
                pid_t pid = fork();
                if (pid == 0) {
                    // Child process
                    // Detach from parent's process group to avoid being killed with parent
                    setsid();
                    
                    // Set process name
                    char process_title[64];
                    snprintf(process_title, sizeof(process_title), "Task-%d", i + 1);
                    
                    // Execute command with task identifier
                    execl("/bin/sh", process_title, "-c", task_cmd, NULL);
                    exit(1);
                } else if (pid > 0) {
                    tasks[i].last_pid = pid;
                    char timestamp[64];
                    time_t now = time(NULL);
                    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));
                    printf("Started task %d with PID: %d at %s\n", i + 1, pid, timestamp);
                    
                    // Log to file for reference
                    FILE *fp = fopen("task_log.txt", "a");
                    if (fp) {
                        fprintf(fp, "Task %d (PID: %d) started at %s: %s\n", 
                                i + 1, pid, timestamp, tasks[i].command);
                        fclose(fp);
                    }
                }
            }
        }
        pthread_mutex_unlock(&task_mutex);
        
        // Clean any zombie processes that might have been missed by the signal handler
        while (waitpid(-1, NULL, WNOHANG) > 0);
        
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
    task->is_demo_task = 0; // Default olarak demo görevi değil
    
    int task_index = task_count; // Save the task index for later use
    task_count++;
    
    // Eğer scheduler zaten çalışıyorsa, yeni görev için hemen bir PID ata
    if (scheduler_running) {
        // Kontrol süreci oluştur
        char task_id[64];
        snprintf(task_id, sizeof(task_id), "Task-%d-Controller", task_index + 1);
        
        // Görev komutunu hazırla
        char command[512];
        
        // Kontrol süreci için komut - bekler ama görev çalıştırmaz, sadece PID için
        snprintf(command, sizeof(command), 
                "echo '[%s] PID: $$ waiting for execution time' > /dev/null & sleep 1", 
                task_id);
        
        // Süreci fork() ile oluştur
        pid_t pid = fork();
        if (pid == 0) {
            // Çocuk süreç - kontrol süreci
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
            printf("Task %d controller started with PID: %d at %s\n", task_index + 1, pid, timestamp);
        }
    }
    
    pthread_mutex_unlock(&task_mutex);
}

// Demo görevi ekleme fonksiyonu - demo için uzun süre çalışacak görev
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
    printf("\n================== Scheduled Tasks ==================\n");
    printf("%-4s %-35s %-10s %-20s %-10s %-12s %-15s\n", 
           "ID", "Command", "Type", "Next Run", "Interval", "Status", "PID (Status)");
    printf("-------------------------------------------------------------------------------------------\n");
    
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
        
        // Enhanced PID status check
        char pid_str[32] = "N/A";
        if (tasks[i].last_pid > 0) {
            // Get process status if running
            char status_cmd[128];
            char status_file[64];
            snprintf(status_file, sizeof(status_file), "/tmp/task_status_%d.tmp", getpid());
            
            // Get process state with ps
            snprintf(status_cmd, sizeof(status_cmd), 
                    "ps -p %d -o state=,comm= > %s 2>/dev/null", 
                    tasks[i].last_pid, status_file);
            system(status_cmd);
            
            // Read the state
            FILE *fp = fopen(status_file, "r");
            if (fp) {
                char state[128] = "";
                if (fgets(state, sizeof(state), fp) != NULL) {
                    // Trim whitespace
                    char *end = state + strlen(state) - 1;
                    while (end > state && isspace(*end)) *end-- = '\0';
                    
                    if (strlen(state) > 0) {
                        // Check if this is a waiting controller process
                        if (strstr(state, "Controller") != NULL) {
                            snprintf(pid_str, sizeof(pid_str), "%d (Waiting)", tasks[i].last_pid);
                        } else {
                            // Regular process with state
                            char state_letter = state[0];
                            snprintf(pid_str, sizeof(pid_str), "%d (%c)", tasks[i].last_pid, state_letter);
                        }
                    } else {
                        // No state means process is gone
                        snprintf(pid_str, sizeof(pid_str), "%d (Ended)", tasks[i].last_pid);
                    }
                } else {
                    snprintf(pid_str, sizeof(pid_str), "%d (Ended)", tasks[i].last_pid);
                }
                fclose(fp);
            } else {
                snprintf(pid_str, sizeof(pid_str), "%d (Unknown)", tasks[i].last_pid);
            }
            
            // Clean up temp file
            unlink(status_file);
        }
        
        // Calculate time remaining
        char status_str[20];
        if (tasks[i].is_active) {
            time_t now = time(NULL);
            long seconds_to_run = tasks[i].execution_time - now;
            
            if (seconds_to_run <= 0) {
                strcpy(status_str, "Due");
            } else if (seconds_to_run < 60) {
                snprintf(status_str, sizeof(status_str), "In %lds", seconds_to_run);
            } else if (seconds_to_run < 3600) {
                snprintf(status_str, sizeof(status_str), "In %ldm", seconds_to_run / 60);
            } else if (seconds_to_run < 86400) {
                snprintf(status_str, sizeof(status_str), "In %ldh", seconds_to_run / 3600);
            } else {
                snprintf(status_str, sizeof(status_str), "In %ldd", seconds_to_run / 86400);
            }
        } else {
            strcpy(status_str, "Inactive");
        }
        
        // Print task information with color highlighting for active tasks
        if (tasks[i].is_active) {
            printf("\033[1;32m"); // Bright green for active tasks
        }
        
        printf("%-4d %-35s %-10s %-20s %-10d %-12s %-15s\n", 
               i + 1,
               tasks[i].command,
               type_str,
               time_str,
               tasks[i].interval_seconds,
               status_str,
               pid_str);
        
        if (tasks[i].is_active) {
            printf("\033[0m"); // Reset color
        }
    }
    
    // Hiç görev yoksa bilgi ver
    if (task_count == 0) {
        printf("\nHenüz hiç görev eklenmemiş. Görev eklemek için '2. Add new task' seçeneğini kullanın.\n");
    } else {
        printf("\n-------------------------------------------------------------------------------------------\n");
        printf("PID durumları: Z = Zombie/Defunct, S = Sleeping, R = Running, Waiting = PID atanmış bekliyor\n");
        printf("Task listesini güncellemek için bu ekranı tekrar açın.\n");
        printf("Görevleri aramak için Task Scheduler menüsündeki 'Filter tasks by name' seçeneğini kullanın.\n");
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
        
        // Scheduler başlatıldığında aktif tüm görevlere hemen PID ata
        pthread_mutex_lock(&task_mutex);
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].is_active && tasks[i].last_pid == 0) {
                // Kontrol süreci oluştur - gerçek görevi çalıştırmaz ancak bir PID'ye sahip olur
                char task_id[64];
                snprintf(task_id, sizeof(task_id), "Task-%d-Controller", i + 1);
                
                // Görev komutunu hazırla
                char command[512];
                
                // Görevin tipine göre bekleme süresi belirle
                time_t now = time(NULL);
                long wait_seconds = 0;
                
                if (tasks[i].type == ONCE || tasks[i].type == DAILY) {
                    wait_seconds = tasks[i].execution_time - now;
                    if (wait_seconds < 0) {
                        if (tasks[i].type == ONCE) {
                            // Zaman geçmişse ve tek seferlik ise, görev pasif olarak işaretlenir
                            tasks[i].is_active = 0;
                            continue;
                        } else if (tasks[i].type == DAILY) {
                            // Daily görev için tarih/saat kontrolü:
                            // Sadece saat/dakika değil, tam günü kontrol edelim
                            struct tm* tm_current = localtime(&now);
                            struct tm* tm_scheduled = localtime(&tasks[i].execution_time);
                            
                            // Aynı güne ait değilse, bu görevi çalıştırma
                            if (tm_current->tm_year != tm_scheduled->tm_year ||
                                tm_current->tm_mon != tm_scheduled->tm_mon || 
                                tm_current->tm_mday != tm_scheduled->tm_mday) {
                                // İleri bir tarih için zamanlanmış, bir sonraki günü bekle
                                continue;
                            }
                            
                            // Aynı gün ama saat geçmişse, bir sonraki güne ayarla
                            if (tm_current->tm_hour > tm_scheduled->tm_hour || 
                                (tm_current->tm_hour == tm_scheduled->tm_hour && 
                                 tm_current->tm_min > tm_scheduled->tm_min)) {
                                tasks[i].execution_time += 24 * 60 * 60;
                                continue;
                            }
                        }
                    }
                } else if (tasks[i].type == INTERVAL) {
                    wait_seconds = tasks[i].interval_seconds;
                }
                
                // Kontrol süreci için komut - bekler ama görev çalıştırmaz, sadece PID için
                snprintf(command, sizeof(command), 
                        "echo '[%s] PID: $$ waiting for execution time' > /dev/null & sleep 1", 
                        task_id);
                
                // Süreci fork() ile oluştur
                pid_t pid = fork();
                if (pid == 0) {
                    // Çocuk süreç - kontrol süreci
                    setsid(); // Kendi süreç grubunu oluştur
                    
                    // Komutun çalıştırılması
                    execl("/bin/sh", task_id, "-c", command, NULL);
                    exit(0);
                } else if (pid > 0) {
                    // Ebeveyn süreç - PID'yi kaydet
                    tasks[i].last_pid = pid;
                    char timestamp[32];
                    time_t now = time(NULL);
                    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));
                    printf("Task %d controller started with PID: %d at %s\n", i + 1, pid, timestamp);
                }
            }
        }
        pthread_mutex_unlock(&task_mutex);
        
        // Scheduler thread'ini başlat
        pthread_create(&scheduler_thread, NULL, scheduler_thread_function, NULL);
        printf("Task scheduler started successfully.\n");
        printf("All tasks now have assigned PIDs which can be viewed in the task list.\n");
    } else {
        printf("Task scheduler is already running.\n");
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
    
    // Önce scheduler task'larını kontrol et
    int task_matches = 0;
    pthread_mutex_lock(&task_mutex);
    
    // Task scheduler başlık bilgisini ekle
    printf("\n--- Task Scheduler Processes Matching '%s' ---\n", name);
    printf("  PID  PPID USER     %%CPU %%MEM STAT COMMAND\n");
    
    // Her bir task için adında name içeriyor mu kontrol et
    for (int i = 0; i < task_count; i++) {
        // Görev adı büyük-küçük harf duyarsız karşılaştır
        char lower_cmd[256];
        char lower_name[256];
        
        strncpy(lower_cmd, tasks[i].command, sizeof(lower_cmd) - 1);
        lower_cmd[sizeof(lower_cmd) - 1] = '\0';
        
        strncpy(lower_name, name, sizeof(lower_name) - 1);
        lower_name[sizeof(lower_name) - 1] = '\0';
        
        // Küçük harfe çevir
        for (int k = 0; lower_cmd[k]; k++) {
            lower_cmd[k] = tolower(lower_cmd[k]);
        }
        
        for (int k = 0; lower_name[k]; k++) {
            lower_name[k] = tolower(lower_name[k]);
        }
        
        if (strstr(lower_cmd, lower_name) != NULL && tasks[i].last_pid > 0) {
            // Görev tipi
            char type_str[10];
            switch (tasks[i].type) {
                case ONCE: strcpy(type_str, "Once"); break;
                case INTERVAL: strcpy(type_str, "Interval"); break;
                case DAILY: strcpy(type_str, "Daily"); break;
                default: strcpy(type_str, "Unknown"); break;
            }
            
            char status = tasks[i].is_active ? 'R' : 'S';
            
            // Süreç durumunu kontrol et
            char status_cmd[128];
            char status_file[64];
            snprintf(status_file, sizeof(status_file), "/tmp/task_status_%d.tmp", getpid());
            
            // ps ile süreç durumunu al
            snprintf(status_cmd, sizeof(status_cmd), 
                    "ps -p %d -o state= > %s 2>/dev/null", 
                    tasks[i].last_pid, status_file);
            system(status_cmd);
            
            // Durumu oku
            FILE *fp = fopen(status_file, "r");
            if (fp) {
                char state[10] = "";
                if (fgets(state, sizeof(state), fp) != NULL && strlen(state) > 0) {
                    status = state[0];
                }
                fclose(fp);
                unlink(status_file);
            }
            
            printf(" %5d %4d %-8s n/a  n/a  %c    Task %d: %s (%s)\n", 
                   tasks[i].last_pid, 
                   getpid(),
                   getenv("USER") ? getenv("USER") : "user",
                   status,
                   i + 1, 
                   tasks[i].command,
                   type_str);
            
            task_matches++;
        }
    }
    
    if (task_matches == 0) {
        printf("No task processes found matching '%s'\n", name);
    } else {
        printf("\nFound %d task scheduler processes matching '%s'\n", task_matches, name);
    }
    
    pthread_mutex_unlock(&task_mutex);
    
    // Şimdi normal sistem süreçlerini kontrol et
    char command[512];
    snprintf(command, sizeof(command), "ps aux | grep -i \"%s\" | grep -v grep", safe_name);
    
    printf("\n--- System Processes Matching '%s' ---\n", name);
    
    int system_matches = 0;
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return;
    } else if (pid == 0) {
        // Child process - redirect stderr to /dev/null to avoid clutter
        freopen("/dev/null", "w", stderr);
        execlp("sh", "sh", "-c", command, NULL);
        // If we get here, exec failed
        _exit(EXIT_FAILURE);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            if (errno == ECHILD) {
                // Child process may have been reaped by signal handler
                // Try to continue by assuming the command succeeded
                // This is a workaround for the "No child processes" error
                printf("Process search completed, but child status unavailable.\n");
                return;
            } else {
                perror("waitpid failed");
                return;
            }
        }
        
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 1) {
                printf("No system processes found matching '%s'\n", safe_name);
            } else if (WEXITSTATUS(status) != 0) {
                printf("Command failed with status: %d\n", WEXITSTATUS(status));
            } else {
                // Komut başarıyla tamamlandıysa ve grep sonuç döndürdüyse
                system_matches = 1;
            }
        } else {
            printf("Process terminated abnormally\n");
        }
    }
    
    // Toplam eşleşme sayısı
    int total_matches = task_matches + system_matches;
    
    if (total_matches == 0) {
        printf("\nNo processes (task or system) found matching '%s'\n", name);
        
        if (strcasestr(name, "task") != NULL) {
            printf("\nNot: TaskManager henüz hiçbir görevi çalıştırmıyor olabilir.\n");
            printf("Task Scheduler'dan (seçenek 11) bir görev ekleyin ve scheduler'ı başlatın.\n");
        }
    }
}

int find_process_by_pid(pid_t target_pid) {
    if (target_pid <= 0) {
        printf("Invalid PID\n");
        return 0;
    }
    
    // İlk olarak task scheduler'da bu PID var mı kontrol et
    pthread_mutex_lock(&task_mutex);
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].last_pid == target_pid) {
            char type_str[10];
            switch (tasks[i].type) {
                case ONCE: strcpy(type_str, "Once"); break;
                case INTERVAL: strcpy(type_str, "Interval"); break;
                case DAILY: strcpy(type_str, "Daily"); break;
                default: strcpy(type_str, "Unknown"); break;
            }
            
            printf("\n  PID  PPID USER  %%CPU %%MEM STAT COMMAND\n");
            printf(" %5d %4d %s   n/a  n/a  %c    Task Scheduler: %s task - %s\n", 
                   target_pid, 
                   getpid(), // Parent PID olarak task manager PID'sini kullan
                   getenv("USER") ? getenv("USER") : "user",
                   tasks[i].is_active ? 'R' : 'S',
                   type_str,
                   tasks[i].command);
            
            pthread_mutex_unlock(&task_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&task_mutex);
    
    // Task scheduler'da bulunamadıysa normal sistem PID kontrolüne devam et
    // First check if the process exists using kill with signal 0 (doesn't send a signal)
    if (kill(target_pid, 0) != 0) {
        if (errno == ESRCH) {
            printf("Process with PID %d not found\n", target_pid);
        } else if (errno == EPERM) {
            printf("Permission denied to access process %d\n", target_pid);
        } else {
            perror("Error checking process");
        }
        return 0;
    }
    
    char command[512];
    snprintf(command, sizeof(command), "ps -p %d -o pid,ppid,user,%%cpu,%%mem,state,command", target_pid);
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return 0;
    } else if (pid == 0) {
        // Child process
        // Redirect stderr to /dev/null to avoid unnecessary error messages
        freopen("/dev/null", "w", stderr);
        execlp("sh", "sh", "-c", command, NULL);
        
        // If exec fails, exit with error code
        _exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        pid_t result = waitpid(pid, &status, 0);
        
        if (result == -1) {
            if (errno == ECHILD) {
                // No child processes - this can happen if the child process has already been reaped
                // Continue as if the process exists since we verified it with kill(pid, 0) earlier
                return 1;
            } else {
                perror("waitpid failed");
                return 0;
            }
        }
        
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                printf("Process with PID %d not found\n", target_pid);
                return 0;
            }
        } else if (WIFSIGNALED(status)) {
            printf("Command terminated by signal %d\n", WTERMSIG(status));
            return 0;
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
    snprintf(command, sizeof(command), "renice %d -p %d", priority, target_pid);
    
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
    
    // First check if process exists
    if (kill(target_pid, 0) != 0) {
        return 0;
    }
    
    // Initialize info structure - zeroing out and setting default values
    memset(info, 0, sizeof(ProcessInfo));
    info->pid = target_pid;
    info->ppid = 0;
    info->state = '?';
    info->username = NULL;
    info->command = NULL;
    info->cpu_percent = 0.0;
    info->mem_percent = 0.0;
    info->memory_kb = 0;
    info->start_time = NULL;
    
    // Try to get process info from /proc for Linux systems
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", target_pid);
    
    FILE *stat_file = fopen(proc_path, "r");
    if (stat_file) {
        // Format is complex, we'll extract what we need
        char state;
        char comm[256];
        pid_t ppid;
        
        if (fscanf(stat_file, "%*d %255s %c %d", 
                  comm, &state, &ppid) >= 3) {
            
            // Remove parentheses from comm
            size_t len = strlen(comm);
            if (len >= 2 && comm[0] == '(' && comm[len-1] == ')') {
                comm[len-1] = '\0';
                memmove(comm, comm+1, len-1);
            }
            
            info->command = strdup(comm);
            info->state = state;
            info->ppid = ppid;
        }
        fclose(stat_file);
    }
    
    // Get memory info from /proc/PID/statm
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/statm", target_pid);
    FILE *mem_file = fopen(proc_path, "r");
    if (mem_file) {
        unsigned long size, resident;
        if (fscanf(mem_file, "%lu %lu", &size, &resident) == 2) {
            // Convert to KB (pages * page size / 1024)
            long page_size = sysconf(_SC_PAGESIZE);
            info->memory_kb = (resident * page_size) / 1024;
        }
        fclose(mem_file);
    }
    
    // Get username from /proc/PID/status
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/status", target_pid);
    FILE *status_file = fopen(proc_path, "r");
    if (status_file) {
        char line[256];
        while (fgets(line, sizeof(line), status_file)) {
            unsigned uid;
            if (sscanf(line, "Uid: %u", &uid) == 1) {
                struct passwd *pw = getpwuid(uid);
                if (pw) {
                    info->username = strdup(pw->pw_name);
                } else {
                    char uid_str[16];
                    snprintf(uid_str, sizeof(uid_str), "%u", uid);
                    info->username = strdup(uid_str);
                }
                break;
            }
        }
        fclose(status_file);
    }
    
    // If /proc is not available (like on macOS), fallback to using ps command
    if (info->command == NULL || strlen(info->command) == 0) {
        char command[512];
        snprintf(command, sizeof(command), 
                "ps -p %d -o user,pid,ppid,%%cpu,%%mem,rss,lstart,state,command | tail -n 1", 
                target_pid);
        
        FILE *proc = popen(command, "r");
        if (proc) {
            char output[1024] = {0};
            if (fgets(output, sizeof(output), proc) != NULL) {
                // Parse the ps output
                char user[64], state[8], cmd[512], start_time[128];
                pid_t pid, parent_pid;
                float cpu_percent, mem_percent;
                unsigned long rss;
                
                // Format string might need adjustment based on actual ps output format
                if (sscanf(output, "%63s %d %d %f %f %lu %127[^]] %7s %511[^\n]", 
                           user, &pid, &parent_pid, &cpu_percent, &mem_percent, 
                           &rss, start_time, state, cmd) >= 8) {
                    
                    if (info->username == NULL) info->username = strdup(user);
                    if (info->command == NULL) info->command = strdup(cmd);
                    info->pid = pid;
                    info->ppid = parent_pid;
                    info->cpu_percent = cpu_percent;
                    info->mem_percent = mem_percent;
                    info->memory_kb = rss;
                    info->state = state[0];
                    info->start_time = strdup(start_time);
                }
            }
            pclose(proc);
        }
    }
    
    return 1;
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
                snprintf(command, sizeof(command), "wmic process where \"Name like '%%%s%%'\" get ProcessId /format:list | findstr /R \"ProcessId=\"");
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
    printf("\n=============== Tasks matching '%s' ===============\n", name);
    printf("%-4s %-35s %-12s %-15s %-15s\n", 
           "ID", "Command", "Status", "PID (State)", "Next Run");
    printf("----------------------------------------------------------------------\n");
    
    int found = 0;
    for (int i = 0; i < task_count; i++) {
        // Case-insensitive search in command
        char lower_cmd[256];
        char lower_name[256];
        
        strncpy(lower_cmd, tasks[i].command, sizeof(lower_cmd) - 1);
        lower_cmd[sizeof(lower_cmd) - 1] = '\0';
        
        strncpy(lower_name, name, sizeof(lower_name) - 1);
        lower_name[sizeof(lower_name) - 1] = '\0';
        
        // Convert both to lowercase
        for (int j = 0; lower_cmd[j]; j++) {
            lower_cmd[j] = tolower(lower_cmd[j]);
        }
        
        for (int j = 0; lower_name[j]; j++) {
            lower_name[j] = tolower(lower_name[j]);
        }
        
        if (strstr(lower_cmd, lower_name) != NULL) {
            // Enhanced PID status check
            char pid_str[32] = "N/A";
            if (tasks[i].last_pid > 0) {
                // Get process status if running
                char status_cmd[128];
                char status_file[64];
                snprintf(status_file, sizeof(status_file), "/tmp/task_status_%d.tmp", getpid());
                
                // Get process state with ps
                snprintf(status_cmd, sizeof(status_cmd), 
                        "ps -p %d -o state=,comm= > %s 2>/dev/null", 
                        tasks[i].last_pid, status_file);
                system(status_cmd);
                
                // Read the state
                FILE *fp = fopen(status_file, "r");
                if (fp) {
                    char state[128] = "";
                    if (fgets(state, sizeof(state), fp) != NULL) {
                        // Trim whitespace
                        char *end = state + strlen(state) - 1;
                        while (end > state && isspace(*end)) *end-- = '\0';
                        
                        if (strlen(state) > 0) {
                            // Check if this is a waiting controller process
                            if (strstr(state, "Controller") != NULL) {
                                snprintf(pid_str, sizeof(pid_str), "%d (Waiting)", tasks[i].last_pid);
                            } else {
                                // Regular process with state
                                char state_letter = state[0];
                                snprintf(pid_str, sizeof(pid_str), "%d (%c)", tasks[i].last_pid, state_letter);
                            }
                        } else {
                            // No state means process is gone
                            snprintf(pid_str, sizeof(pid_str), "%d (Ended)", tasks[i].last_pid);
                        }
                    } else {
                        snprintf(pid_str, sizeof(pid_str), "%d (Ended)", tasks[i].last_pid);
                    }
                    fclose(fp);
                } else {
                    snprintf(pid_str, sizeof(pid_str), "%d (Unknown)", tasks[i].last_pid);
                }
                
                // Clean up temp file
                unlink(status_file);
            }
            
            // Calculate time remaining
            char status_str[20];
            if (tasks[i].is_active) {
                time_t now = time(NULL);
                long seconds_to_run = tasks[i].execution_time - now;
                
                if (seconds_to_run <= 0) {
                    strcpy(status_str, "Due");
                } else if (seconds_to_run < 60) {
                    snprintf(status_str, sizeof(status_str), "In %lds", seconds_to_run);
                } else if (seconds_to_run < 3600) {
                    snprintf(status_str, sizeof(status_str), "In %ldm", seconds_to_run / 60);
                } else if (seconds_to_run < 86400) {
                    snprintf(status_str, sizeof(status_str), "In %ldh", seconds_to_run / 3600);
                } else {
                    snprintf(status_str, sizeof(status_str), "In %ldd", seconds_to_run / 86400);
                }
            } else {
                strcpy(status_str, "Inactive");
            }
            
            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", 
                    localtime(&tasks[i].execution_time));
            
            // Highlight active tasks
            if (tasks[i].is_active) {
                printf("\033[1;32m"); // Bright green for active tasks
            }
            
            printf("%-4d %-35s %-12s %-15s %-15s\n", 
                   i + 1,
                   tasks[i].command,
                   status_str,
                   pid_str,
                   time_str);
                   
            if (tasks[i].is_active) {
                printf("\033[0m"); // Reset color
            }
            
            found++;
        }
    }
    
    if (!found) {
        printf("No tasks found matching '%s'\n", name);
        printf("\nTask eklemek için Task Scheduler menüsünden '2. Add new task' seçeneğini kullanın.\n");
    } else {
        printf("\n----------------------------------------------------------------------\n");
        printf("Toplam %d görev bulundu. PID durumları: Z = Zombie/Defunct, S = Sleeping, R = Running, Waiting = Bekliyor\n", found);
        printf("Görevleri ve durumlarını ayrıntılı incelemek için 'list scheduled tasks' kullanabilirsiniz.\n");
    }
    pthread_mutex_unlock(&task_mutex);
} 
