# TaskManager

A comprehensive command-line process management and task scheduling tool for Unix-like systems.

![Version](https://img.shields.io/badge/version-1.0-blue)
![Platform](https://img.shields.io/badge/platform-Unix%20%7C%20macOS-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

## Features

- **Process Management**: List, filter, and control system processes
- **Task Scheduler**: Create and manage scheduled tasks with custom timing
- **Priority Management**: Easily adjust process priorities
- **Resource Monitoring**: Track CPU and memory usage
- **Process Visualization**: Display process trees and hierarchies
- **Group Operations**: Perform batch actions on multiple processes
- **Chat Application**: Built-in messaging system
- **Interactive Interface**: User-friendly menu-driven operation

## Key Highlights

- 💻 **Modern Process Manager**: Full-featured tool for Unix/Linux/macOS systems
- ⏱️ **Flexible Task Scheduling**: Schedule one-time, interval, or daily recurring tasks
- 🔄 **Priority Control**: Fine-tune CPU allocation for optimal performance
- 📊 **Comprehensive Monitoring**: Detailed view of system resource utilization

## Installation

### Prerequisites

- GCC compiler
- Make
- POSIX-compliant system (Linux, macOS, or other Unix-like OS)
- ncurses library (for TUI components)

### Building from Source

1. Clone the repository:
```bash
git clone https://github.com/YOUR_USERNAME/TaskManager.git
cd TaskManager
```

2. Compile the application:
```bash
make
```

3. (Optional) Install to your local bin directory:
```bash
make install
```

## Quick Start Guide

1. Launch TaskManager:
```bash
./process_manager
```

2. The main menu offers several options. Navigate using the number keys.

3. **Creating Scheduled Tasks**:
   - Select "11. Task Scheduler"
   - Choose "2. Add new task"
   - Enter a command to execute
   - Select timing (once, interval, or daily)
   - Configure execution time

4. **Managing Process Priorities**:
   - From main menu, choose "5. Change process priority"
   - Enter the PID of the process
   - Set priority value (-20 to 19, lower is higher priority)
   - Alternatively, use "9. Group operations" for batch changes

## Task Scheduling & Priority Management

TaskManager excels at task automation and performance optimization:

### Task Scheduler

The scheduler can run tasks:
- **Once**: At a specific date and time
- **Intervals**: Every X seconds repeatedly
- **Daily**: At the same time each day

Tasks are created with sequential IDs (1, 2, 3...) for easy management within the scheduler.

### Priority Management

When tasks run, they get system-assigned PIDs which can be used to adjust priorities:
- **High Priority** (-10): For CPU-intensive tasks that need to complete quickly
- **Normal Priority** (0): Default balanced setting
- **Low Priority** (10): For background tasks that shouldn't impact system performance

## Demonstration

Watch TaskManager in action:

[View Demo Video](./demo.mp4)

## Documentation

For complete documentation, see the [User Manual](./usermanual.md).

## Implementation Details

TaskManager is built using:
- Process management system calls (fork, exec, wait, kill)
- FIFO pipes for inter-process communication
- Terminal control with the termios library
- ncurses for terminal UI components

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Author

Created by kappasutra

---

**Note**: Some operations may require administrative privileges, especially when managing processes that don't belong to your user. 