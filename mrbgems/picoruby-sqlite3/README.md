# picoruby-sqlite3

SQLite3 database library for PicoRuby - embedded SQL database.

## Overview

Provides SQLite3 database functionality for PicoRuby with VFS integration.

## Usage

```ruby
require 'sqlite3'
require 'vfs'
require 'filesystem-fat'

# Set up VFS for SQLite
SQLite3.vfs_methods = FAT::VFSMethods
SQLite3.time_methods = Time::TimeMethods

# Create/open database
db = SQLite3::Database.new("/sd/mydb.sqlite3")

# Create table
db.execute("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")

# Insert data
db.execute("INSERT INTO users (name, age) VALUES (?, ?)", "Alice", 30)
db.execute("INSERT INTO users VALUES (?, ?, ?)", 2, "Bob", 25)

# Query data
rows = db.execute("SELECT * FROM users")
rows.each do |row|
  puts "ID: #{row[0]}, Name: #{row[1]}, Age: #{row[2]}"
end

# Query with block
db.execute("SELECT * FROM users WHERE age > ?", 25) do |row|
  puts "Name: #{row[1]}"
end

# Close database
db.close
```

## API

### Class Methods

- `SQLite3.vfs_methods=(methods)` - Set VFS methods for file operations
- `SQLite3.time_methods=(methods)` - Set time methods for timestamp functions

### Database Methods

- `SQLite3::Database.new(filename)` - Open/create database
- `execute(sql, *bindings)` - Execute SQL with optional bindings
- `execute(sql, *bindings) { |row| }` - Execute with block iteration
- `close()` - Close database

## Supported Types

- **Integer** - SQLite INTEGER
- **Float** - SQLite REAL
- **String** - SQLite TEXT
- **nil** - SQLite NULL
- **bool** - SQLite INTEGER (0/1)

## Notes

- Requires VFS (Virtual File System) support
- Database files stored on filesystem (SD card, flash, etc.)
- Uses prepared statements for parameter binding
- Prevents SQL injection when using bindings
- Lightweight embedded database (no server required)
- Full SQL support (CREATE, SELECT, INSERT, UPDATE, DELETE, etc.)
