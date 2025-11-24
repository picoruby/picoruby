# picoruby-filesystem-fat

FAT filesystem support for PicoRuby - SD card and flash storage with FAT32/FAT16.

## Overview

Provides FAT filesystem driver for VFS, enabling file operations on SD cards and flash storage.

## Usage

```ruby
require 'vfs'
require 'filesystem-fat'

# Create FAT driver for SD card
fat = FAT.new(
  spi_unit: :RP2040_SPI1,
  sck_pin: 10,
  cipo_pin: 12,
  copi_pin: 11,
  cs_pin: 13
)

# Mount filesystem
VFS.mount(fat, "/sd")

# Now use standard file operations
File.open("/sd/test.txt", "w") do |f|
  f.write("Hello from PicoRuby!")
end

# Read file
content = File.read("/sd/test.txt")
puts content

# List directory
Dir.entries("/sd").each { |e| puts e }

# Unmount when done
VFS.unmount(fat)
```

## API

### FAT Driver

- `FAT.new(spi_unit:, sck_pin:, cipo_pin:, copi_pin:, cs_pin:)` - Create FAT driver
- Works with VFS for file operations

### VFS Methods

- `FAT::VFSMethods` - VFS method set for SQLite3 integration

## Features

- FAT16 and FAT32 support
- SD card support via SPI
- Long filename support
- Directory operations
- File read/write/append
- Timestamp support

## Notes

- Requires SPI peripheral for SD card
- Must mount via VFS before use
- Compatible with SQLite3 for database storage
- Supports standard Ruby File and Dir operations
- Typical SD card uses FAT32 format
