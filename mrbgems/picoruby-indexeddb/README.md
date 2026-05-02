# picoruby-indexeddb

Ruby-idiomatic IndexedDB wrapper for PicoRuby.wasm.

## Features

- Simple, synchronous-looking API using `await` under the hood
- Schema versioning with upgrade callbacks
- In-memory fallback when IndexedDB is unavailable (e.g., testing environments)

## Usage

```ruby
require 'indexeddb'

# Check availability
IndexedDB.available?  # => true (in browser)

# Open a database with schema upgrade
db = IndexedDB.open('mydb', version: 1) do |db, old_version, new_version|
  db.create_store('users', key_path: 'id') if old_version < 1
end

# Introspection
db.store_names      # => ["users"]
db.has_store?('users')  # => true

# Close when done
db.close
```

### Schema Upgrades

The upgrade block runs only when creating a new database or bumping the version:

```ruby
db = IndexedDB.open('mydb', version: 2) do |db, old_v, new_v|
  db.create_store('users', key_path: 'id') if old_v < 1
  db.create_store('cache', auto_increment: true) if old_v < 2
end
```

**Important:** Schema mutations (`create_store`, `delete_store`) are only valid inside the upgrade block. Calling them outside raises `IndexedDB::SchemaError`.

### In-Memory Fallback

When `IndexedDB.available?` returns `false`, `IndexedDB.open` automatically falls back to an in-memory implementation with the same API:

```ruby
# Force no fallback (raises NotSupportedError if unavailable)
db = IndexedDB.open('mydb', version: 1, fallback: false)
```

## API Reference

### Module Methods

| Method | Description |
|--------|-------------|
| `IndexedDB.available?` | Returns `true` if browser IndexedDB is accessible |
| `IndexedDB.open(name, version:, fallback:, &block)` | Opens a database, optionally running upgrade block |

### Database

| Method | Description |
|--------|-------------|
| `name` | Database name |
| `version` | Current schema version |
| `store_names` | Array of object store names |
| `has_store?(name)` | Check if store exists |
| `store(name)` | Get a Store object |
| `create_store(name, key_path:, auto_increment:)` | Create object store (upgrade only) |
| `delete_store(name)` | Delete object store (upgrade only) |
| `close` | Close the database connection |
| `closed?` | Returns `true` if closed |

## Errors

| Error | Description |
|-------|-------------|
| `IndexedDB::Error` | Base error class |
| `IndexedDB::NotSupportedError` | IndexedDB unavailable and fallback disabled |
| `IndexedDB::OpenError` | Failed to open database |
| `IndexedDB::SchemaError` | Schema mutation outside upgrade block |

## Dependencies

- `picoruby-wasm`
- `picoruby-json`

## License

MIT
