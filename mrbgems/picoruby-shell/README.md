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
- `recv` - Receive a file over serial with a known byte size
- `run` - Run a Ruby file and print a completion marker
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

## Serial Upload/Run Workflow

For R2P2-based targets, the shell can be used as a minimal serial upload/run target:

- `recv <path> <size>` receives a file over serial using a chunked ACK protocol and saves it to `<path>`
- `run <path>` loads the file and always prints `__PICORUBY_RUN_DONE__` when execution ends

A host-side helper script is available at:

- `tools/r2p2_serial_run.rb`

Typical flow:

1. Connect to the Pico shell and make sure it is idle at the shell prompt.
2. Close `screen`/`cu` so the serial port is free.
3. From the host, run:

```sh
ruby tools/r2p2_serial_run.rb /dev/cu.usbmodemXXXX examples/hello.rb
```

This sends the file to `/home/app.rb`, runs it with `run /home/app.rb`, streams `puts` output back to the host, and stops reading when `__PICORUBY_RUN_DONE__` is received.

### Transfer protocol

The current `recv` implementation is intentionally conservative and prioritizes reliability:

1. Host sends `recv <path> <size>`
2. Device replies `READY chunk=<chunk_size>`
3. Host sends one chunk
4. Device writes the chunk to `<path>.part`
5. Device replies `ACK <offset>`
6. After the last chunk, device renames `<path>.part` to `<path>` and replies `OK <size>`

Before receiving data, the device removes any stale `<path>.part` file and checks free space on the target volume. If there is not enough room, it replies `ERR no space: need=<bytes> free=<bytes>` without starting the transfer.

If anything else fails, the device replies `ERR <message>` and removes the temporary file.

### Useful options

- `--skip-run` uploads the file but does not call `run`
- `--chunk-timeout-ms MS` adjusts the per-chunk ACK timeout

### Large-file verification samples

You can generate larger Ruby files for transfer testing with:

```sh
ruby tools/generate_serial_transfer_samples.rb
```

By default this writes:

- `/private/tmp/picoruby-serial-transfer-samples/sample_12kb.rb`
- `/private/tmp/picoruby-serial-transfer-samples/sample_100kb.rb`
- `/private/tmp/picoruby-serial-transfer-samples/sample_1mb.rb`

These files are valid Ruby programs that print a short summary when run.
