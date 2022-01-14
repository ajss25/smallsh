# smallsh

smallsh is a bash-like shell written in C, with a limited subset of features.

### Features

- Prompt for running commands.
- Ignore blank commands and comments beginning with `#`.
- Expansion for variable `$$` into PID of smallsh itself.
- Implement `cd`, `exit`, and `status` commands.
- Execute other commands by creating new processes using `exec` family of functions.
- Support I/O redirection.
- Support running commands in foreground and background processes.
- Custom handlers for `SIGINT` and `SIGTSTP` signals.

### How to Use

- Compile smallsh with gcc:
  - `gcc --std=gnu99 -o smallsh smallsh.c`
  - `./smallsh`
- General syntax of a command line:
  - `command [arg1 arg2 ...] [< input_file] [> output_file] [&]`
