# picoruby-rng

Random Number Generator (RNG) using hardware entropy for PicoRuby.

## Usage

```ruby
require 'rng'

# Generate random integer
num = RNG.random_int
puts num  # => Random 32-bit integer

# Generate random string
str = RNG.random_string(16)  # 16 bytes
puts str.length  # => 16

# Generate UUID (v4 format)
uuid = RNG.uuid
puts uuid  # => "550e8400-e29b-41d4-a716-446655440000"
```

## API

### Methods

- `RNG.random_int()` - Generate random Integer (32-bit)
- `RNG.random_string(length)` - Generate random binary string of specified length
- `RNG.uuid()` - Generate UUID v4 string

## Use Cases

- **Cryptographic Operations**: Secure random data for encryption
- **Session IDs**: Generate unique session identifiers
- **UUIDs**: Create universally unique identifiers
- **Tokens**: Generate random tokens for authentication
- **Testing**: Random test data generation

## Notes

- Uses hardware random number generator for true randomness
- More secure than pseudo-random number generators
- UUIDs follow RFC 4122 version 4 format
- Random strings contain binary data (not printable characters)
- Suitable for cryptographic applications
