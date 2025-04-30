#include <mach/mach.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <libproc.h>
#include <errno.h>
#include <pwd.h>

void list_threads_of_process(pid_t pid) {
    task_t task;
    kern_return_t kr;

    // Hedef prosesin task portunu al
    kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        return;
    }

    thread_act_array_t thread_list;
    mach_msg_type_number_t thread_count;

    // Thread listesini al
    kr = task_threads(task, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
        return;
    }

    printf("PID %d için %d thread found:\n", pid, thread_count);
    for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
        thread_basic_info_data_t info;
        mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
        kr = thread_info(thread_list[i], THREAD_BASIC_INFO, (thread_info_t)&info, &info_count);
        if (kr == KERN_SUCCESS) {
            printf("Thread %u: state: %s, user_time: %u.%06u sec\n",
                   thread_list[i],
                   (info.run_state == TH_STATE_RUNNING) ? "RUNNING" :
                   (info.run_state == TH_STATE_STOPPED) ? "STOPPED" :
                   (info.run_state == TH_STATE_WAITING) ? "WAITING" :
                   (info.run_state == TH_STATE_UNINTERRUPTIBLE) ? "UNINTERRUPTIBLE" :
                   (info.run_state == TH_STATE_HALTED) ? "HALTED" : "UNKNOWN",
                   info.user_time.seconds, info.user_time.microseconds);
        }
        mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list, thread_count * sizeof(thread_act_t));
}

// Helper function to get process owner
static uid_t get_process_owner(pid_t pid) {
    struct proc_bsdinfo proc;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc, sizeof(proc)) > 0) {
        return proc.pbi_uid;
    }
    return -1;
}

// Helper function to check if we can access a process
static int can_access_process(pid_t pid) {
    uid_t current_euid = geteuid();
    uid_t proc_owner = get_process_owner(pid);
    
    // Root can access everything
    if (current_euid == 0) return 1;
    
    // Process owner can access their own processes
    if (proc_owner != -1 && current_euid == proc_owner) return 1;
    
    // Try alternative methods if direct access fails
    struct proc_taskinfo pti;
    if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti)) > 0) {
        return 1;
    }
    
    return 0;
}

// Alternative method to get thread info using proc_pidinfo directly
static int get_thread_info_proc(pid_t pid, int *count, char *summary, size_t summary_size) {
    struct proc_taskinfo pti;
    
    if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti)) == sizeof(pti)) {
        if (count) *count = pti.pti_threadnum;
        if (summary && summary_size > 0) {
            snprintf(summary, summary_size, "%d threads", pti.pti_threadnum);
        }
        return 1;
    }
    return 0;
}

// Helper for table: fills thread_count and a summary string (id:STATE, ...)
void get_thread_summary_for_table(pid_t pid, int *thread_count, char *summary, size_t summary_size) {
    // Initialize outputs
    if (thread_count) *thread_count = 0;
    if (summary && summary_size > 0) summary[0] = '\0';
    
    // First check if we can access the process
    if (!can_access_process(pid)) {
        // Try alternative methods first
        if (get_thread_info_proc(pid, thread_count, summary, summary_size)) {
            return;
        }
        
        // If all methods fail, indicate limited access
        if (summary && summary_size > 0) {
            snprintf(summary, summary_size, "Limited info");
        }
        return;
    }
    
    // Try to get detailed thread info using Mach tasks
    task_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        // Fall back to basic info if Mach task access fails
        if (get_thread_info_proc(pid, thread_count, summary, summary_size)) {
            return;
        }
        return;
    }

    thread_act_array_t thread_list;
    mach_msg_type_number_t tcount;
    kr = task_threads(task, &thread_list, &tcount);
    
    if (kr != KERN_SUCCESS) {
        mach_port_deallocate(mach_task_self(), task);
        return;
    }

    // Set thread count
    if (thread_count) *thread_count = tcount;

    // Build summary string with detailed state information
    if (summary && summary_size > 0) {
        char *ptr = summary;
        size_t left = summary_size;
        int written = 0;
        
        for (mach_msg_type_number_t i = 0; i < tcount && left > 1; i++) {
            thread_basic_info_data_t info;
            mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
            kr = thread_info(thread_list[i], THREAD_BASIC_INFO, (thread_info_t)&info, &info_count);
            
            if (kr == KERN_SUCCESS) {
                const char *state = 
                    (info.run_state == TH_STATE_RUNNING) ? "RUN" :
                    (info.run_state == TH_STATE_STOPPED) ? "STOP" :
                    (info.run_state == TH_STATE_WAITING) ? "WAIT" :
                    (info.run_state == TH_STATE_UNINTERRUPTIBLE) ? "UNINT" :
                    (info.run_state == TH_STATE_HALTED) ? "HALT" : "UNK";
                
                // Add CPU usage if thread is running
                if (info.run_state == TH_STATE_RUNNING) {
                    written = snprintf(ptr, left, "%u:%s(%.1f%%)%s", 
                        thread_list[i], state,
                        (info.cpu_usage / 10.0),
                        (i < tcount-1) ? ", " : "");
                } else {
                    written = snprintf(ptr, left, "%u:%s%s", 
                        thread_list[i], state,
                        (i < tcount-1) ? ", " : "");
                }
                
                if (written < 0 || (size_t)written >= left) break;
                ptr += written;
                left -= written;
            }
        }
        
        // If we couldn't get detailed info but know the count
        if (summary[0] == '\0' && tcount > 0) {
            snprintf(summary, summary_size, "%d active threads", (int)tcount);
        }
    }

    // Cleanup
    for (mach_msg_type_number_t i = 0; i < tcount; i++) {
        mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list, tcount * sizeof(thread_act_t));
    mach_port_deallocate(mach_task_self(), task);
}