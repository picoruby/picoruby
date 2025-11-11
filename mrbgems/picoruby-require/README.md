# picoruby-require

Dynamic library loading for PicoRuby (require/load functionality).

## Usage

```ruby
# Require a library (loads only once)
require 'json'
require 'json'  # Won't load again

# Load a file (loads every time)
load '/path/to/script.rb'

# Check what has been loaded
puts $LOADED_FEATURES.inspect

# Require relative (for CRuby compatibility)
require_relative './helper'
```

## API

### Methods

- `require(name)` - Load library by name (only once), returns true if loaded
- `load(path)` - Load file by path (every time), returns true
- `require_relative(path)` - Require file relative to current file

### Global Variables

- `$LOADED_FEATURES` - Array of already loaded library names/paths

## How It Works

### require(name)

1. Checks if already in `$LOADED_FEATURES`
2. Searches for file in load paths:
   - `./{name}.rb`
   - `./{name}.mrb`
   - `/lib/{name}.rb`
   - `/lib/{name}.mrb`
3. Loads file if found and adds to `$LOADED_FEATURES`
4. Returns `true` if loaded, `false` if already loaded
5. Raises `LoadError` if file not found

### load(path)

1. Loads file at exact path specified
2. Always loads, even if previously loaded
3. Does not add to `$LOADED_FEATURES`
4. Raises `LoadError` if file not found

## Notes

- `.mrb` files are pre-compiled mruby bytecode
- `.rb` files are Ruby source code
- `require` is preferred for libraries (loads once)
- `load` is useful for scripts that should run multiple times
