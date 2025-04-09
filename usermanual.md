# TASKMANAGER USER MANUAL

## OVERVIEW

TaskManager is a comprehensive command-line tool for managing processes in Unix-like systems. It allows users to monitor, control, and schedule tasks with an intuitive interface.

### Key Features:

- List and filter running processes
- Find processes by PID or name
- Terminate processes
- Adjust process priorities
- View process hierarchies and states
- Monitor resource usage (CPU/memory)
- Schedule and manage automated tasks
- Built-in Chat Application
- Group operations on multiple processes

## GETTING STARTED

### Building the Application

To compile the standard release version:
```bash
make
```

For debugging:
```bash
make debug
```

### Installation (Optional)

To install to your local bin directory:
```bash
make install
```

### Running TaskManager

After building, launch the application with:
```bash
./process_manager
```

## MAIN MENU

When TaskManager starts, the following menu options are displayed:

1. List all processes - Shows all processes running on the system
2. Filter processes by name - Finds processes matching a specific pattern
3. Find process by PID - Looks up details about a process with a specific PID
4. Terminate a process - Sends signals to terminate processes
5. Change process priority - Adjusts the priority (nice value) of a process
6. Show process states information - Displays detailed information about process states
7. Display process tree - Shows parent-child relationships
8. Show top resource usage - Lists processes sorted by CPU or memory usage
9. Group operations - Performs operations on multiple processes matching criteria
10. Chat App - Launches the messaging application
11. Task Scheduler - Manages scheduled tasks
0. Exit - Quits the program

## TASK SCHEDULER GUIDE

The Task Scheduler allows you to create, schedule, and manage automated tasks. This feature is ideal for recurring system maintenance, monitoring, or any command you want to run on a schedule.

### Types of Scheduling:

1. **Run Once**: Executes the task once at a specified date and time
2. **Run at Intervals**: Runs the task repeatedly at set intervals (e.g., every 60 seconds)
3. **Run Daily**: Executes the task daily at a specific time

### Important Concept: Task IDs vs Process IDs

The Task Scheduler uses two different identifier systems:

- **Task ID**: Sequential numbers (1, 2, 3...) assigned when tasks are created. Used within the Task Scheduler menu for managing tasks.
- **Process ID (PID)**: Unique system-assigned identifiers given to tasks when they are actually running. Used for changing priorities and other system-level operations.

### Creating and Managing Tasks - Step by Step Guide

#### Step 1: Launch TaskManager
```bash
./process_manager
```

#### Step 2: Adding Your First Task
1. Select "11. Task Scheduler" from the main menu
2. Choose "2. Add new task"
3. Enter a command, for example: `echo "This is my first task" >> task_log.txt`
4. Select scheduling type: "2. Run at intervals"
5. Enter interval in seconds: `60` (runs every minute)
6. The task will be created with Task ID 1

#### Step 3: Adding a Second Task
1. Select "2. Add new task" again
2. Enter another command: `ls -la >> system_files.txt`
3. Select scheduling type: "3. Run daily at specific time"
4. Enter execution time in format "YYYY MM DD HH MM"
5. The task will be created with Task ID 2

#### Step 4: Adding a Third Task
1. Select "2. Add new task" again
2. Enter another command: `ps aux | grep firefox > browser_status.txt`
3. Select scheduling type: "1. Run once at specific time"
4. Enter execution time in format "YYYY MM DD HH MM"
5. The task will be created with Task ID 3

#### Step 5: Viewing Your Tasks
1. Select "1. List scheduled tasks"
2. You'll see all your tasks listed with their Task IDs (1, 2, 3)
3. These Task IDs are used for removing or modifying tasks within the scheduler

#### Step 6: Starting the Scheduler
1. Select "4. Start scheduler"
2. Tasks will begin executing according to their schedules
3. Each running task becomes a process with its own system-assigned PID

#### Step 7: Finding the Process IDs of Running Tasks
1. Return to the main menu by selecting "0. Back to main menu"
2. From the main menu, select "1. List all processes"
3. You can filter to see your task processes:
   - Select "2. Filter processes by name"
   - Enter part of your command as a filter (e.g., "echo" or "task")
4. Note the PID numbers of your running tasks (these are different from Task IDs)

#### Step 8: Changing Task Priorities
1. From the main menu, select "5. Change process priority"
2. Enter the PID of the task whose priority you want to change (not the Task ID)
3. Enter a new priority value:
   - High priority: `-10` (gives more CPU access)
   - Normal priority: `0`
   - Low priority: `10` (gives CPU time to other processes)
4. The task's priority will be updated immediately

#### Step 9: Removing a Task
1. Return to Task Scheduler by selecting "11. Task Scheduler"
2. Select "3. Remove task"
3. Enter the Task ID (1, 2, or 3) of the task you want to remove
4. The task will be removed from the scheduler

### Task Management Tips

- Task IDs (1, 2, 3...) are only used within the Task Scheduler
- PIDs are system-wide unique identifiers for running processes
- To see running tasks, use "1. List all processes" from the main menu
- When the scheduler is stopped, all task processes will terminate
- When restarted, new processes will be created with new PIDs
- Important tasks should be given higher priority (lower nice value)
- Check that commands in your tasks are executable from your shell

## CHANGING PROCESS PRIORITIES

TaskManager allows you to adjust the priority of any running process. In Unix systems, priorities range from `-20` (highest priority) to `19` (lowest priority).

### Changing Priority for a Single Process

1. Select "5. Change process priority" from the main menu
2. Enter the PID of the process
3. Enter a new priority value between -20 and 19

Examples:
- High priority: `-10`
- Normal priority: `0`
- Low priority: `10`

### Batch Priority Changes

1. Select "9. Group operations" from the main menu
2. Choose pattern type:
   - 1. Process name
   - 2. Username
   - 3. Process state
3. Enter a pattern to match (e.g., "bash")
4. Select operation: "2. Change priority"
5. Enter a new priority value for all matching processes

## OTHER FEATURES

### Process State Information
Select "6. Show process states information" from the main menu to learn about various process states:
- R: Running - Process is running or runnable
- S: Sleeping - Process is in interruptible sleep
- D: Uninterruptible Sleep - Process is in uninterruptible sleep (usually I/O)
- Z: Zombie - Process has terminated but not reaped by its parent
- T: Stopped - Process is stopped (on a signal)

### Process Tree View
Select "7. Display process tree" to visualize the process hierarchy:
1. Show all processes
2. Show tree for a specific PID

### Resource Monitoring
Select "8. Show top resource usage" to see processes using the most resources:
1. Sort by CPU usage
2. Sort by memory usage

### Chat Application
TaskManager includes a built-in messaging system with server and client components. Access it through option "10. Chat App" in the main menu.

## TROUBLESHOOTING

If you encounter issues:
1. Compile with debug symbols: `make debug`
2. Run with GDB: `gdb ./process_manager`
3. Check error messages for clues

### Common Issues:
- Some operations require administrative privileges
- Process priority changes may require sudo access
- Make sure commands in scheduled tasks have the correct paths

---
TaskManager v1.0 | Created by kappasutra | Documentation for GitHub 