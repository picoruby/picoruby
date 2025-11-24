# picoruby-editor

Text editor component for PicoRuby.

## Overview

Provides text editing capabilities for PicoRuby applications.

## Usage

```ruby
require 'editor'

# Create editor instance
editor = Editor.new

# Load file
editor.load("file.txt")

# Edit operations
editor.insert("text")
editor.delete
editor.move_cursor(row, col)

# Save file
editor.save("file.txt")
```

## Features

- Text insertion and deletion
- Cursor movement
- File loading and saving
- Line operations
- Multi-line editing

## Use Cases

- Building text editors
- Configuration file editors
- Code editors for embedded systems
- Log viewers with editing

## Notes

- Can be used as component for larger applications
- Requires display/terminal for UI
- Works with VFS for file operations
- Simpler alternative to full Vim implementation
