# picoruby-posix-io

POSIX I/O operations for PicoRuby - File and IO operations.

## Overview

Provides POSIX-compatible file I/O operations for PicoRuby.

## Usage

```ruby
require 'posix-io'

# File operations (typically through VFS)
File.open("file.txt", "w") do |f|
  f.write("Hello, World!\n")
  f.puts("Another line")
end

# Read entire file
content = File.read("file.txt")
puts content

# Read with block
File.open("file.txt", "r") do |f|
  while line = f.gets
    puts line
  end
end

# File info
if File.exist?("file.txt")
  size = File.size("file.txt")
  puts "File size: #{size} bytes"
end

# File operations
File.rename("old.txt", "new.txt")
File.delete("unwanted.txt")

# IO operations
io = IO.new(fd)
data = io.read(100)
io.write("data")
io.close
```

## API

### File Class Methods

- `File.open(path, mode)` - Open file
- `File.read(path)` - Read entire file
- `File.write(path, data)` - Write to file
- `File.exist?(path)` - Check if file exists
- `File.size(path)` - Get file size
- `File.rename(old, new)` - Rename file
- `File.delete(path)` - Delete file

### File Modes

- `"r"` - Read only
- `"w"` - Write (truncate)
- `"a"` - Append
- `"r+"` - Read/write
- `"w+"` - Read/write (truncate)
- `"a+"` - Read/append

### IO Instance Methods

- `read(length = nil)` - Read data
- `write(string)` - Write data
- `gets()` - Read line
- `puts(string)` - Write line
- `close()` - Close IO
- `closed?()` - Check if closed

## Notes

- Works with VFS for filesystem access
- Provides CRuby-compatible API
- Buffered I/O for efficiency
- Binary and text mode support
