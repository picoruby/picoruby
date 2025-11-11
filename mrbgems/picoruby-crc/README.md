# picoruby-crc

CRC (Cyclic Redundancy Check) calculation library for PicoRuby.

## Usage

```ruby
require 'crc'

# Calculate CRC32 for string
data = "Hello, World!"
crc = CRC.crc32(data)
puts "CRC32: 0x#{crc.to_s(16)}"

# Calculate CRC32 with initial value
crc1 = CRC.crc32("Hello")
crc2 = CRC.crc32(", World!", crc1)  # Continue from previous CRC

# Calculate CRC32 from memory address
# (useful for checking data integrity in memory)
crc = CRC.crc32_from_address(address, length)
```

## API

### Methods

- `CRC.crc32(string = "", crc = 0)` - Calculate CRC32 checksum
  - `string`: Data to calculate CRC for
  - `crc`: Initial CRC value (default: 0)
  - Returns: CRC32 value as Integer

- `CRC.crc32_from_address(address, length, crc = 0)` - Calculate CRC32 from memory
  - `address`: Memory address as Integer
  - `length`: Number of bytes to process
  - `crc`: Initial CRC value (default: 0)
  - Returns: CRC32 value as Integer

## Notes

- CRC32 is commonly used for data integrity verification
- Can process data incrementally by passing previous CRC value
