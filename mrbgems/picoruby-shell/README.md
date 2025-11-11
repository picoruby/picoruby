# picoruby-shell

Interactive shell for PicoRuby - command-line interface and REPL.

## Overview

Provides an interactive shell/REPL for PicoRuby with command execution and Ruby evaluation.

## Features

- **REPL**: Interactive Ruby evaluation
- **File System**: Navigate and manage files
- **Command Execution**: Run shell-like commands
- **History**: Command history support
- **Tab Completion**: Auto-completion (if available)

## Usage

```ruby
require 'shell'

# Start shell
Shell.start
```

## Built-in Commands

Typically includes commands like:
- `ls` - List files
- `cd` - Change directory
- `pwd` - Print working directory
- `cat` - Display file contents
- `mkdir` - Create directory
- `rm` - Remove file
- `help` - Show help

## REPL Mode

Type Ruby code directly:
```ruby
> 1 + 2
=> 3

> name = "PicoRuby"
=> "PicoRuby"

> puts "Hello, #{name}!"
Hello, PicoRuby!
=> nil
```

## Notes

- Useful for interactive development
- Debugging embedded systems
- System administration
- Educational purposes
- Requires VFS for file operations
- Commands are implemented in `shell_executables/` directory
