# picoruby-vfs

Virtual File System (VFS) layer for PicoRuby - provides unified interface for multiple file systems.

## Usage

```ruby
require 'vfs'

# Mount a filesystem
VFS.mount(fat_driver, "/sd")
VFS.mount(flash_driver, "/flash")

# Change directory
VFS.chdir("/sd/data")
puts VFS.pwd  # => "/sd/data"

# Create directory
VFS.mkdir("/sd/newdir")

# Check if file/directory exists
if VFS.exist?("/sd/file.txt")
  puts "File exists"
end

if VFS.directory?("/sd/data")
  puts "Is a directory"
end

# File operations
VFS::File.open("/sd/file.txt", "w") do |f|
  f.write("Hello")
end

# Directory operations
dir = VFS::Dir.open("/sd")
dir.each { |entry| puts entry }
dir.close

# Rename files
VFS.rename("/sd/old.txt", "/sd/new.txt")

# Delete files
VFS.unlink("/sd/file.txt")

# Change permissions
VFS.chmod(0644, "/sd/file.txt")

# Unmount filesystem
VFS.unmount(fat_driver)
```

## API

### VFS Methods

- `VFS.mount(driver, mountpoint)` - Mount filesystem at mountpoint
- `VFS.unmount(driver, force = false)` - Unmount filesystem
- `VFS.chdir(dir)` - Change current directory
- `VFS.pwd()` - Get current working directory
- `VFS.mkdir(path, mode = 0777)` - Create directory
- `VFS.unlink(path)` - Delete file
- `VFS.rename(from, to)` - Rename file/directory
- `VFS.chmod(mode, path)` - Change file permissions
- `VFS.exist?(path)` - Check if path exists
- `VFS.directory?(path)` - Check if path is directory
- `VFS.contiguous?(path)` - Check if file is contiguous

### VFS::File

- `VFS::File.open(path, mode)` - Open file
- `VFS::File.utime(atime, mtime, *filenames)` - Update file times

### VFS::Dir

- `VFS::Dir.open(path)` - Open directory

## Notes

- Mountpoints must start with `/`
- Cannot unmount filesystem if PWD is within it (unless `force: true`)
- Paths are automatically resolved relative to PWD
- Supports `..` and `.` in paths
- Cannot rename across different mounted filesystems
- First mounted filesystem becomes default if no mountpoint matches
