# picoruby-base16

Base16 (hexadecimal) encoding/decoding library for PicoRuby.

## Usage

```ruby
require 'base16'

# Encode to Base16
encoded = Base16.encode16("Hello")
puts encoded  # => "48656C6C6F"

# Decode from Base16
decoded = Base16.decode16("48656C6C6F")
puts decoded  # => "Hello"
```

## API

### Methods

- `Base16.encode16(string)` - Encode string to Base16 (hex) format
- `Base16.decode16(string)` - Decode Base16 (hex) string

## Notes

- Base16 encoding represents binary data as hexadecimal digits
- Output is uppercase hexadecimal characters (0-9, A-F)
