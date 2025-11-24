# picoruby-vim

Vim-like text editor for PicoRuby.

## Overview

Provides a Vi/Vim-inspired text editor that runs on embedded systems with PicoRuby.

## Usage

```ruby
require 'vim'

# Launch editor
Vim.edit("filename.txt")
```

## Features

- **Modal Editing**: Normal, Insert, Command modes
- **Vi-like Commands**: Familiar Vi/Vim keybindings
- **File Operations**: Open, save, quit
- **Navigation**: Character, word, line movement
- **Editing**: Insert, delete, change text
- **Search**: Find text in file

## Common Commands

### Normal Mode
- `i` - Enter insert mode
- `h,j,k,l` - Move cursor left, down, up, right
- `w,b` - Move word forward/backward
- `x` - Delete character
- `dd` - Delete line
- `:` - Enter command mode

### Insert Mode
- `ESC` - Return to normal mode
- Type to insert text

### Command Mode
- `:w` - Save file
- `:q` - Quit
- `:wq` - Save and quit
- `/pattern` - Search

## Notes

- Lightweight editor for embedded systems
- Subset of Vim features
- Useful for editing configuration files on device
- Requires terminal/console support
- Works with VFS for file access
