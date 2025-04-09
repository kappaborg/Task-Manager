#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>

typedef struct {
    char code;
    char *description;
} ProcessState;

typedef struct {
    pid_t pid;
    pid_t ppid;
    char state;
    char *username;
    char *command;
    float cpu_percent;
    float mem_percent;
    unsigned long memory_kb;
    char *start_time;
} ProcessInfo;

/**
 * Lists all processes running on the system
 */
void list_all_processes(void);

/**
 * Filters processes by name
 * @param name The name to filter by
 */
void filter_processes_by_name(const char *name);

/**
 * Finds a process by PID
 * @param pid The process ID to find
 * @return 1 if found, 0 otherwise
 */
int find_process_by_pid(pid_t pid);

/**
 * Terminates a process by sending a SIGTERM signal
 * @param pid The process ID to terminate
 * @return 1 if successful, 0 otherwise
 */
int terminate_process(pid_t pid);

/**
 * Changes the priority of a process
 * @param pid The process ID
 * @param priority The new priority (-20 to 19, lower is higher priority)
 * @return 1 if successful, 0 otherwise
 */
int change_process_priority(pid_t pid, int priority);

void show_process_states_info(void);

/**
 * Displays a process tree showing parent-child relationships
 * @param root_pid The PID to use as the root of the tree (0 for all processes)
 */
void display_process_tree(pid_t root_pid);

/**
 * Shows processes sorted by resource usage (CPU or memory)
 * @param sort_by 1 for CPU, 2 for memory
 * @param count Number of processes to show (top N)
 */
void show_top_resource_usage(int sort_by, int count);

/**
 * Gets process info for a specific PID
 * @param pid The process ID
 * @param info Pointer to ProcessInfo structure to fill
 * @return 1 if successful, 0 otherwise
 */
int get_process_info(pid_t pid, ProcessInfo *info);

/**
 * Perform operations on groups of processes
 * @param pattern Pattern to match (name, user, etc.)
 * @param pattern_type 1 for name, 2 for user, 3 for state
 * @param operation 1 for terminate, 2 for change priority
 * @param param Additional parameter (priority value if operation is 2)
 * @return Number of processes affected
 */
int process_group_operation(const char *pattern, int pattern_type, int operation, int param);

/**
 * Shows explanation for process states
 * @param state The state code to explain
 */
void explain_process_state(char state);

/**
 * Free memory allocated for ProcessInfo structure
 * @param info The ProcessInfo structure to free
 */
void free_process_info(ProcessInfo *info);

// Task Scheduler yapıları
typedef enum {
    ONCE,           // Bir kere çalıştır
    INTERVAL,       // Belirli aralıklarla çalıştır
    DAILY          // Her gün belirli saatte çalıştır
} schedule_type_t;

typedef struct {
    char command[256];           // Çalıştırılacak komut
    schedule_type_t type;        // Zamanlama tipi
    time_t execution_time;       // Çalıştırma zamanı
    int interval_seconds;        // INTERVAL tipi için aralık
    int is_active;              // Task aktif mi?
    pid_t last_pid;             // Son çalıştırılan process'in PID'i
    int is_demo_task;           // Demo görevi mi?
} scheduled_task_t;

// Task Scheduler fonksiyonları
void init_task_scheduler(void);
void add_scheduled_task(const char* command, schedule_type_t type, time_t execution_time, int interval_seconds);
void add_demo_task(const char* name);
void list_scheduled_tasks(void);
void remove_scheduled_task(int task_index);
void run_task_scheduler(void);
void stop_task_scheduler(void);
void filter_tasks_by_name(const char* name);

#endif 
