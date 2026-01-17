# wsh-shell - Unix Shell Implementation (C)

`wsh` is a Unix-style shell implemented in C that supports interactive and batch execution, process creation, pipelines, and several built-in commands. The project focuses on understanding how a real shell works under the hood by directly using the Unix Process API instead of relying on wrapper libraries.

This shell was built from scratch with an emphasis on correctness, memory safety, and clean separation of responsibilities between parsing, execution, and state management.

---

## Features

### Execution Modes
- **Interactive mode** with a prompt (`wsh>`)
- **Batch mode** for executing commands from a script file

### External Commands
- Executes programs using `fork`, `execv`, and `wait`
- Searches executables using the `PATH` environment variable
- Supports absolute and relative paths
- Graceful error handling when commands are missing or not executable

### Built-in Commands
Implemented directly inside the shell without forking:
- `exit` – cleanly terminates the shell
- `cd` – changes the current working directory
- `path` – views or updates the PATH variable
- `alias` / `unalias` – command aliasing with overwrite support
- `which` – resolves whether a command is an alias, builtin, or executable
- `history` – stores and queries command history for the current session

### Pipelines
- Supports pipelines with up to 128 segments
- Executes all pipeline stages concurrently
- Uses `pipe` and `dup2` to connect stdout and stdin correctly
- Carefully closes unused file descriptors to avoid deadlocks

Example:
```sh
ls -l | grep .c | wc -l
