# Smallsh

Smallsh is a bash-like shell written in C, with a limited subset of features.

### Features

- Prompt for running commands
- Ignore blank commands and comments beginning with `#`
- Variable expansion
- Implement `cd`, `exit`, and `status` commands
- Execute other commands by creating new processes
- Supports I/O redirection
- Supports running commands in foreground and background processes
- Custom handlers for `SIGTINT` and `SIGTSTP` signals

### How to Use

- Compile smallsh with gcc:
  - `gcc --std=gnu99 -o smallsh smallsh.c`
  - `./smallsh`
- General syntax of a command line:
  - `command [arg1 arg2 ...] [< input_file] [> output_file] [&]`
