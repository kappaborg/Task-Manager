# Task-Manager

A comprehensive process management and task scheduling tool for Unix-like systems.

We have a simple example for "HOW TO USE" down below

![Task Manager Demo](TaskManager/assets/demo.gif)

![Version](https://img.shields.io/badge/version-1.0-blue)
![Platform](https://img.shields.io/badge/platform-Unix%20%7C%20macOS-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

## About This Project

TaskManager is a powerful command-line utility that allows users to monitor system processes, schedule tasks, and manage process priorities through an intuitive interface. This tool is designed for system administrators, developers, and power users who need fine-grained control over their system's processes.

## Main Features

- 💻 **Process Management**: List, filter, find, and terminate processes
- ⏱️ **Task Scheduling**: Create and manage scheduled tasks with flexible timing options
- 🔄 **Priority Control**: Adjust process priorities for optimal performance
- 📊 **Resource Monitoring**: View processes sorted by CPU or memory usage
- 🌳 **Process Tree**: Visualize parent-child relationships between processes
- 👥 **Group Operations**: Perform batch actions on multiple processes
- 💬 **Chat Application**: Built-in messaging system for team communication

## Installation

To build and install TaskManager:

```bash
# Clone this repository
git clone https://github.com/kappaborg/Task-Manager.git

# Navigate to the project directory
cd Task-Manager/TaskManager

# Compile
make

# Optional: Install to your system
make install
```

## Documentation

For detailed usage instructions, please see the [User Manual](TaskManager/usermanual.md).

## Project Structure

- `TaskManager/` - Contains the main application code
  - `main.c` - Core program logic
  - `process_manager.c` - Process management functionality
  - `process_manager.h` - Header definitions
  - `Chat App/` - Built-in messaging application
  - `Makefile` - Build configuration
  - `usermanual.md` - Complete user documentation


## License

This project is licensed under the MIT License.

## Author

Created by kappasutra

---

**Note**: For the most up-to-date documentation, please refer to the files in the `TaskManager` directory. 
