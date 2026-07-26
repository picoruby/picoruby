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

## picoruby.wasm (browser)

On picoruby.wasm there is no filesystem, and SQLite's synchronous VFS callbacks
cannot block on the browser's async storage. So on wasm the working database is
kept **in memory** and its bytes are **snapshotted to IndexedDB** at await
points. The `name` passed to `.new` is the IndexedDB key rather than a path, and
no VFS is mounted.

```ruby
require 'sqlite3'

# Opens an in-memory database and restores a prior snapshot if one exists
db = SQLite3::Database.new('my_app')
db.execute('CREATE TABLE IF NOT EXISTS notes (id INTEGER PRIMARY KEY, body TEXT)')
db.execute('INSERT INTO notes (body) VALUES (?)', ['hello'])

db.persist   # snapshot the database into IndexedDB under 'my_app'
db.close     # close also auto-persists
```

- `persist` and `close` are the only durability points; writes since the last
  `persist` are lost on a crash or page reload. Persistence happens at Ruby
  `await` points, never inside a SQLite VFS callback, so it cooperates with the
  Task scheduler.
- `serialize` / `deserialize` expose the raw snapshot bytes if you want to store
  them elsewhere.
- The database is bounded by available wasm memory.

The rest of the API (execute, transactions, pragmas, backup, ...) is identical
to the microcontroller build.

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

### Transactions

Wrapping many writes in one transaction turns them into a single commit, which
is the main way to cut flash write wear on a microcontroller:

```ruby
db.transaction do |t|
  1000.times { |i| t.execute("INSERT INTO log (n) VALUES (?)", [i]) }
end
# Committed once here; rolled back automatically if the block raises
```

`commit` / `rollback` / `transaction_active?` are also available for manual
control.

### Backup

Copy a database into another open database (for example, snapshot a working
database to a second file on flash):

```ruby
dst = SQLite3::Database.new("/backup.sqlite3")
db.backup(dst)   # copies every page in one step
dst.close
```

The lower level `SQLite3::Backup` class (`new`, `step`, `finish`, `remaining`,
`pagecount`) is available when you want to copy incrementally.

### Pragmas and migrations

`user_version` is the usual lever for on-device schema migrations, and the
journal/synchronous pragmas trade durability for fewer flash writes:

```ruby
# Cut flash writes: keep the rollback journal in RAM, sync less aggressively
db.journal_mode = :memory
db.synchronous = 0

# Migrate the schema based on a stored version stamp
if db.user_version < 1
  db.execute_batch("CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT);")
  db.user_version = 1
end

db.integrity_check   # => "ok"
```

Other pragmas are reachable with `db.pragma("name")` / `db.pragma("name", value)`
or a raw `execute("PRAGMA ...")`.

## API

### Database Methods

- `SQLite3::Database.new(filename, results_as_hash: false)` - Open/create database
- `execute(sql, bind_vars = [])` - Execute SQL with optional bindings
- `execute(sql, bind_vars = []) { |row| }` - Execute with block iteration
- `execute_batch(sql)` - Run a script of several statements (no bindings)
- `query(sql, bind_vars = [])` - Like `execute` but returns a `ResultSet`
- `get_first_row(sql, bind_vars = [])` - First row, or `nil`
- `get_first_value(sql, bind_vars = [])` - First column of the first row, or `nil`
- `prepare(sql)` - Return a `SQLite3::Statement`
- `transaction(mode = :deferred) { |db| }` / `commit` / `rollback` / `transaction_active?`
- `last_insert_row_id`, `changes`, `total_changes`
- `backup(dst, srcname: "main", dstname: "main")` - Copy into another database
- `readonly?`, `filename`
- `results_as_hash=` - Yield rows as Hash instead of Array
- `close()` / `closed?()`

### Pragma Methods

Convenience accessors for the pragmas most useful on a microcontroller:

- `user_version` / `user_version=` - schema version stamp for migrations
- `journal_mode` / `journal_mode=` - e.g. `:memory` or `:off` to avoid journal writes
- `synchronous` / `synchronous=`
- `cache_size` / `cache_size=`, `page_size` / `page_size=`
- `foreign_keys` / `foreign_keys=`, `auto_vacuum` / `auto_vacuum=`
- `freelist_count`, `integrity_check`
- `pragma(name, value = nil)` - generic query/set for any pragma

### Statement Methods

- `bind_param(key, value)` - `key` is a 1-based index, or the parameter name as a Symbol or String
- `bind_params(*values)` - Bind positionally; a Hash argument binds by name
- `execute(*bind_vars)`, `step`, `reset!`, `done?`, `columns`, `types`, `close`, `closed?`

### Errors

Failures raise `SQLite3::Exception`, a subclass of `StandardError`. A few common
result codes raise a dedicated subclass so they can be rescued individually
(each is still a `SQLite3::Exception`):

| Exception | SQLite result code |
|--------------------------------|--------------------|
| `SQLite3::ConstraintException` | `SQLITE_CONSTRAINT` |
| `SQLite3::BusyException`       | `SQLITE_BUSY`       |
| `SQLite3::ReadOnlyException`   | `SQLITE_READONLY`   |
| `SQLite3::FullException`       | `SQLITE_FULL`       |
| `SQLite3::CorruptException`    | `SQLITE_CORRUPT`    |
| `SQLite3::IOException`         | `SQLITE_IOERR`      |

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

The gem is tested through the standard Picotest harness. The test mounts a
picoruby-littlefs RAM block device, so no board is needed (picoruby-littlefs is
pulled in automatically as a test-only dependency):

```
rake test:gems:picoruby[picoruby-sqlite3]
```
