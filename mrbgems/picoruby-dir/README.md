# picoruby-dir

Directory operations library for PicoRuby.

## Usage

```ruby
require 'dir'

# Get current directory
puts Dir.getwd  # or Dir.pwd

# Change directory
Dir.chdir("/home")
Dir.chdir("/home") { |dir| puts "In #{dir}" }  # With block

# List directory entries
entries = Dir.entries("/home")
entries.each { |e| puts e }

# Iterate over entries
Dir.foreach("/home") { |entry| puts entry }

# Create directory
Dir.mkdir("/home/newdir")
Dir.mkdir("/home/newdir", 0755)  # With permissions

# Delete directory
Dir.delete("/home/emptydir")  # or Dir.rmdir, Dir.unlink

# Check if directory exists
if Dir.exist?("/home")
  puts "Directory exists"
end

# Glob pattern matching
Dir.glob("*.rb") { |file| puts file }
files = Dir.glob("/home/**/*.txt")

# Directory object
dir = Dir.open("/home")
while entry = dir.read
  puts entry
end
dir.close

# With block (auto-close)
Dir.open("/home") do |dir|
  dir.each { |entry| puts entry }
end
```

## API

### Class Methods

- `Dir.chdir(dir = nil)` - Change current directory
- `Dir.getwd()` / `Dir.pwd()` - Get current working directory
- `Dir.entries(dir)` / `Dir.children(dir)` - List directory entries
- `Dir.foreach(path) { }` - Iterate over directory entries
- `Dir.mkdir(dirname, mode = 0777)` - Create directory
- `Dir.delete(dirname)` / `Dir.rmdir(dirname)` / `Dir.unlink(dirname)` - Delete directory
- `Dir.exist?(dirname)` - Check if directory exists
- `Dir.glob(pattern, flags = 0, base: nil)` - Find files matching pattern
- `Dir.open(dirname)` - Open directory

### Instance Methods

- `close()` - Close directory
- `read()` - Read next entry
- `each { }` - Iterate over all entries
- `rewind()` - Rewind to beginning
- `seek(pos)` - Seek to position
- `tell()` / `pos()` - Get current position
- `empty?()` - Check if directory is empty

## Notes

- Works with VFS (Virtual File System) if available
- Directory must be empty to delete
- Glob supports `*` and `**` wildcards
