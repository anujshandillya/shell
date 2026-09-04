# Shell - POSIX System calls

## Overview

This project implements a simple POSIX-style command-line shell in C++17. It
supports built-in commands, external commands, command pipelines, redirection,
and multiple commands separated by semicolons.

## Prerequisites

- A POSIX-compatible operating system such as Linux or macOS
- `g++` with C++17 support
- `make`

## Files & Folders Structure

```
shell/
├── include/
│   ├── command.hpp
│   ├── command_handler.hpp
│   ├── history.hpp
│   ├── parser.hpp
│   ├── redirection.hpp
│   ├── shell.hpp
│   └── terminal.hpp
├── src/
│   ├── command.cpp
│   ├── command_handler.cpp
│   ├── history.cpp
│   ├── main.cpp
│   ├── parser.cpp
│   ├── redirection.cpp
│   ├── shell.cpp
│   └── terminal.cpp
├── Makefile
└── README.md
```

## Build Commands

- `make`: This builds the shell executable using g++ (C++17).
- `make run`: This builds the shell executable file and runs it after.
- `make cleanup`: cleans the linker files(.o)
- `make delete`: deletes the executable build.

## Usage

```bash
# go to the project directory, and run the following.
make
./shell.out
```

You can also build and run the shell with:

```bash
make run
```

## Built-in Commands

The shell currently provides the following built-in commands:

- `cd`: Change the current working directory.
- `ls`: List directory contents.
- `pwd`: Print the current working directory.
- `pinfo`: Display process information.
- `search`: Search for files or directories.
- `history`: Display previously entered commands.

External commands available on the system can also be executed.

## Features

- Built-in commands implemented mainly with POSIX system calls.
- External command execution.
- Multiple commands separated by semicolons.
- Pipelines using `|`.
- Input redirection using `<`.
- Output redirection using `>` and `>>`.

## Examples

```bash
pwd
ls -l
ls -l | grep cpp
cat < input.txt
echo "hello" > output.txt
echo "another line" >> output.txt
pwd; ls
```

## Architecture

- `Shell` manages the interactive loop, terminal input, signals, and command
  processing.
- `Parser` tokenizes input and identifies commands, pipes, and redirection.
- `CommandHandler` dispatches built-in and external commands.
- `History` stores and displays previously entered commands.
- `Redirection` configures input and output file descriptors.
- `Terminal` contains terminal-related functionality.

## Known Limitations

This is a lightweight educational shell and is not intended to provide full
`bash` compatibility. Advanced shell features such as scripting, job control,
environment expansion, and command substitution may not be supported.

## License

No license has been specified for this project.

## Author

Add the project author or contributor information here.