# C-Shell: A Custom Linux Shell Implementation

## Overview
**C-Shell** is a robust, custom-built Linux shell written in C. It demonstrates a deep understanding of low-level system programming, process management, and operating system concepts. Designed to mimic the functionality of standard shells like Bash or Zsh, C-Shell implements core features such as job control, piping, redirection, and signal handling from scratch.

This project serves as a showcase of systems programming expertise, featuring a modular codebase and a custom implementation of shell built-ins.

## Key Features

### 🚀 Core Functionality
- **Command Execution**: Supports execution of both external programs (e.g., `ls`, `grep`) and custom built-in commands.
- **Piping (`|`)**: Implements inter-process communication using pipes, allowing the output of one command to serve as the input for another (e.g., `ls | grep .c`).
- **I/O Redirection**: Full support for input (`<`), output (`>`), and append (`>>`) redirection.
- **Background Execution (`&`)**: Run long-running processes in the background, keeping the shell interactive.

### 🛠️ Advanced Job Control
- **Process Groups**: Manages process groups to correctly handle foreground and background jobs.
- **Signal Handling**: Custom handlers for `SIGINT` (Ctrl+C) and `SIGTSTP` (Ctrl+Z) to manage running processes without killing the shell itself.
- **Job Management**:
  - `activities`: List all active background and stopped jobs.
  - `fg <job_id>`: Bring a background job to the foreground.
  - `bg <job_id>`: Resume a stopped job in the background.
  - `ping <pid> <signal>`: Send custom signals to specific processes.

### ⚡ Custom Built-in Commands
- **`hop`**: A smarter `cd` command.
  - `hop <dir>`: Change directory.
  - `hop -`: Toggle between the current and previous directory.
  - `hop ~`: Go to the home directory.
- **`reveal`**: A custom `ls` implementation.
  - `reveal`: List files in the current directory.
  - `reveal -a`: Show hidden files.
  - `reveal -l`: List in a single column format.
- **`log`**: History management.
  - View command history.
  - Persistent history across sessions (saved to `.mini_shell_history`).

## Getting Started

### Prerequisites
- GCC Compiler
- Linux Environment (or WSL)
- Make build system

### Installation
1. Clone the repository:
   ```bash
   git clone git@github.com:ineshshukla/C-Shell.git
   cd C-Shell
   ```
2. Build the project using `make`:
   ```bash
   make
   ```
   This will compile the source code and generate the `shell.out` executable.

### Running the Shell
Start the shell by running:
```bash
./shell.out
```
You will be greeted with the C-Shell prompt, ready to accept commands.

## Usage Examples

### Navigation and File Listing
```bash
C-Shell> hop src        # Change to 'src' directory
C-Shell> reveal -l      # List files in list format
C-Shell> hop -          # Go back to previous directory
```

### Piping and Redirection
```bash
C-Shell> reveal | grep .c > c_files.txt   # List files, filter for .c, save to file
C-Shell> wc -l < c_files.txt              # Count lines in the saved file
```

### Job Control
```bash
C-Shell> sleep 100 &            # Run sleep in background
C-Shell> activities             # List active jobs
[1] 12345 Running sleep 100
C-Shell> fg 1                   # Bring job 1 to foreground
^Z                              # Suspend with Ctrl+Z
C-Shell> bg 1                   # Resume in background
```

## Technical Highlights
- **Tokenizer & Parser**: Custom implementation to parse complex command lines with multiple pipes and redirections.
- **Memory Management**: Careful allocation and deallocation of resources to prevent memory leaks during long sessions.
- **System Calls**: Extensive use of POSIX system calls including `fork`, `execvp`, `pipe`, `dup2`, `waitpid`, and `sigaction`.

## Author
**Inesh Shukla**
*Systems Programmer & Software Engineer*
