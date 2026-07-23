# picoruby-sqlite3

SQLite3 database library for PicoRuby - embedded SQL database.

## Overview

Provides SQLite3 database functionality for PicoRuby on top of picoruby-vfs.

SQLite's OS layer is implemented as a custom VFS that calls the Ruby methods of
whichever driver picoruby-vfs has mounted, so the database lives on the same
filesystem as everything else and no filesystem specific code is needed here.
picoruby-littlefs is the usual driver.

Requires the mruby VM (`conf.picoruby`); the gem conflicts with
picoruby-mrubyc.

## Usage

```ruby
require 'littlefs'
require 'sqlite3'

VFS.mount(Littlefs.new(:flash), "/")

# Create/open database. The path is resolved through VFS, so it names a
# mounted volume just like File.open would
db = SQLite3::Database.new("/home/mydb.sqlite3")

# Create table
db.execute("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")

# Insert data
db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Alice", 30])
db.execute("INSERT INTO users VALUES (?, ?, ?)", [2, "Bob", 25])

# Insert with named parameters
db.execute("INSERT INTO users (name, age) VALUES (:name, :age)", [{ name: "Carol", age: 41 }])

# Query data
rows = db.execute("SELECT * FROM users")
rows.each do |row|
  puts "ID: #{row[0]}, Name: #{row[1]}, Age: #{row[2]}"
end

# Query with block
db.execute("SELECT * FROM users WHERE age > ?", [25]) do |row|
  puts "Name: #{row[1]}"
end

# Rows as Hash
db.results_as_hash = true
db.execute("SELECT * FROM users") do |row|
  puts row["name"]
end

# Close database
db.close
```

## API

### Database Methods

- `SQLite3::Database.new(filename, results_as_hash: false)` - Open/create database
- `execute(sql, bind_vars = [])` - Execute SQL with optional bindings
- `execute(sql, bind_vars = []) { |row| }` - Execute with block iteration
- `prepare(sql)` - Return a `SQLite3::Statement`
- `results_as_hash=` - Yield rows as Hash instead of Array
- `close()` / `closed?()`

### Statement Methods

- `bind_param(key, value)` - `key` is a 1-based index, or the parameter name as a Symbol or String
- `bind_params(*values)` - Bind positionally; a Hash argument binds by name
- `execute(*bind_vars)`, `step`, `reset!`, `done?`, `columns`, `types`, `close`, `closed?`

### Errors

Failures raise `SQLite3::Exception`, a subclass of `StandardError`.

## Supported Types

- **Integer** - SQLite INTEGER
- **Float** - SQLite REAL
- **String** - SQLite TEXT and BLOB
- **nil** - SQLite NULL
- **true / false** - SQLite INTEGER (1/0)

## Notes

- Requires VFS (Virtual File System) support; the mounted driver supplies the files
- The sector size SQLite uses is taken from the driver's `File#sector_size`
- Uses prepared statements for parameter binding, which prevents SQL injection
- Lightweight embedded database (no server required)

### Limitations

- The VFS driver protocol has no truncate, so `xTruncate` is a no-op. A database
  file therefore never shrinks; `VACUUM` reclaims pages inside the file but does
  not shorten it.
- Locking is a no-op because there is a single process. Do not open the same
  database from two tasks that write.
- Always `close` a database (the block form of `.new` does it for you). Closing
  calls back into the VFS driver, which the GC cannot do, so a database that is
  only garbage collected releases its C handles but leaks the underlying File
  objects until the VM exits.

## Testing

`build_config/sqlite3-test.rb` builds a host binary that runs the gem against
picoruby-littlefs' RAM block device, so no board is needed:

```
MRUBY_CONFIG=sqlite3-test rake
./build/sqlite3-test/bin/sqlite3-test
```
