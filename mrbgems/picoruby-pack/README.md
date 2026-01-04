# picoruby-pack

Array and String pack/unpack methods for PicoRuby.

## Usage

```ruby
require 'pack'

# Pack array into binary string
data = [1, 2, 3, 4].pack("CCCC")  # Pack as unsigned chars

# Pack with different formats
value = [0x12345678].pack("N")  # Network byte order (big-endian) 32-bit
bytes = [255, 128, 64].pack("CCC")  # Three unsigned 8-bit integers

# String.pack (class method)
packed = String.pack("C*", 72, 101, 108, 108, 111)
puts packed  # => "Hello"

# Unpack binary string into an array
unpacked_data = "Hello".unpack("CCCCC")
p unpacked_data  # => [72, 101, 108, 108, 111]

# Unpack with different formats
value = "\x12\x34\x56\x78".unpack("N")
p value # => [305419896]
```

## Common Format Specifiers

- `C` - Unsigned 8-bit integer (0-255)
- `c` - Signed 8-bit integer (-128-127)
- `S` - Unsigned 16-bit integer, native endian
- `s` - Signed 16-bit integer, native endian
- `L` - Unsigned 32-bit integer, native endian
- `l` - Signed 32-bit integer, native endian
- `N` - Unsigned 32-bit integer, network (big-endian) byte order
- `n` - Unsigned 16-bit integer, network (big-endian) byte order
- `V` - Unsigned 32-bit integer, little-endian
- `v` - Unsigned 16-bit integer, little-endian
- `*` - Use all remaining elements

## API

### Methods

- `Array#pack(format)` - Pack array elements into binary string
- `String.pack(format, *args)` - Pack arguments into binary string
- `String#unpack(format)` - Unpack binary string into an array of values
- `Array.unpack(format, *args)` - Unpack arguments into an array of values

## Notes

- Not all CRuby pack formats are supported
- Useful for binary protocol handling and data serialization
