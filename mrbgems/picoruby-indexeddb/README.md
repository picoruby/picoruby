# picoruby-indexeddb

Ruby-idiomatic IndexedDB wrapper for PicoRuby.wasm.

## Features

- Simple, synchronous-looking API using `await` under the hood
- Schema versioning with upgrade callbacks
- One-shot autocommit operations (each method opens its own transaction)
- Secondary index support
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

### Store Operations

```ruby
users = db.store('users')

# Create / Update
users.put({ 'id' => 1, 'name' => 'Alice' })
users.put({ 'id' => 2, 'name' => 'Bob' })

# Read
users.get(1)        # => { 'id' => 1, 'name' => 'Alice' }
users.get(999)      # => nil

# Delete
users.delete(2)

# Count
users.count         # => 1

# Get all keys / values
users.keys          # => [1]
users.to_a          # => [{ 'id' => 1, 'name' => 'Alice' }]

# Iterate
users.each do |key, value|
  puts "#{key}: #{value['name']}"
end

# Clear all
users.clear
```

### Out-of-line Keys

For stores without `key_path`, provide the key explicitly:

```ruby
db = IndexedDB.open('mydb', version: 1) do |db, old_v, new_v|
  db.create_store('kv') if old_v < 1
end

kv = db.store('kv')
kv.put('some value', key: 'mykey')
kv.get('mykey')  # => 'some value'
```

### Schema Upgrades

The upgrade block runs only when creating a new database or bumping the version:

```ruby
db = IndexedDB.open('mydb', version: 2) do |db, old_v, new_v|
  db.create_store('users', key_path: 'id') if old_v < 1
  db.create_store('cache', auto_increment: true) if old_v < 2
end
```

**Important:** Schema mutations (`create_store`, `delete_store`, `create_index`) are only valid inside the upgrade block. Calling them outside raises `IndexedDB::SchemaError`.

### Secondary Indexes

```ruby
db = IndexedDB.open('mydb', version: 2) do |db, old_v, new_v|
  if old_v < 1
    store = db.create_store('users', key_path: 'id')
    store.create_index('by_name', 'name')
  end
end

users = db.store('users')
users.put({ 'id' => 1, 'name' => 'Alice' })

# Query via index
users.index('by_name').get('Alice')  # => { 'id' => 1, 'name' => 'Alice' }
```

### Key Ranges

```ruby
# Exact match
range = IndexedDB::KeyRange.only(5)

# Lower bound (>= 10)
range = IndexedDB::KeyRange.lower_bound(10)

# Upper bound (<= 100)
range = IndexedDB::KeyRange.upper_bound(100)

# Between (10 <= key < 100)
range = IndexedDB::KeyRange.bound(10, 100, exclude_upper: true)

# Use with iteration
users.each(range) { |key, value| ... }
users.keys(range)
users.to_a(range)
```

### In-Memory Fallback

When `IndexedDB.available?` returns `false`, `IndexedDB.open` automatically falls back to an in-memory implementation with the same API. Data is lost on page reload.

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

### Store

| Method | Description |
|--------|-------------|
| `get(key)` | Retrieve value by primary key |
| `put(value, key:)` | Store a value (key extracted from value if key_path set) |
| `delete(key)` | Delete by primary key |
| `count(key_or_range)` | Count records |
| `clear` | Remove all records |
| `keys(range)` | Get all primary keys |
| `to_a(range)` | Get all values |
| `each(range, direction:)` | Iterate over [key, value] pairs |
| `index(name)` | Get an Index object |
| `create_index(name, key_path, unique:, multi_entry:)` | Create index (upgrade only) |
| `delete_index(name)` | Delete index (upgrade only) |

### Index

| Method | Description |
|--------|-------------|
| `get(key)` | Get first matching record |
| `get_all(key_or_range, count:)` | Get all matching records |
| `count(key_or_range)` | Count matching records |
| `keys(range)` | Get all primary keys |
| `to_a(range)` | Get all values |
| `each(range, direction:)` | Iterate over [primary_key, value] pairs |

### KeyRange

| Method | Description |
|--------|-------------|
| `KeyRange.only(value)` | Match exactly one key |
| `KeyRange.lower_bound(lower, exclude:)` | All keys >= lower |
| `KeyRange.upper_bound(upper, exclude:)` | All keys <= upper |
| `KeyRange.bound(lower, upper, exclude_lower:, exclude_upper:)` | Keys between bounds |

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
