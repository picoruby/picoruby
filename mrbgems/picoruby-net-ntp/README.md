# picoruby-net-ntp

Simple NTP (Network Time Protocol) client implementation for PicoRuby.

## Overview

`picoruby-net-ntp` provides a pure Ruby implementation of NTP client compatible with CRuby's basic NTP functionality. It enables embedded systems to synchronize time with NTP servers over UDP.

## Features

- Pure Ruby implementation
- CRuby-compatible API
- UDP-based time synchronization
- Configurable timeout
- Works on both POSIX and RP2040 platforms
- No external dependencies beyond picoruby-socket

## Installation

Add to your `build_config.rb`:

```ruby
conf.gem :core => 'picoruby-net-ntp'
```

## Usage

### Basic Time Query

```ruby
require 'net/ntp'

# Get current time from default NTP server (pool.ntp.org)
unix_time = Net::NTP.get

puts "Current Unix timestamp: #{unix_time}"
```

### Custom NTP Server

```ruby
require 'net/ntp'

# Use specific NTP server with custom timeout
unix_time = Net::NTP.get("time.nist.gov", 123, 10)

# Convert to Time object if available
time = Time.at(unix_time) if defined?(Time)
puts time
```

### With Error Handling

```ruby
require 'net/ntp'

begin
  unix_time = Net::NTP.get("pool.ntp.org", 123, 5)
  puts "Time synchronized: #{unix_time}"
rescue => e
  puts "Failed to get NTP time: #{e.message}"
end
```

## API Reference

### Net::NTP

#### Class Methods

- `Net::NTP.get(host = "pool.ntp.org", port = 123, timeout = 5)` - Get current time from NTP server
  - **host**: NTP server hostname (default: "pool.ntp.org")
  - **port**: NTP server port (default: 123)
  - **timeout**: Timeout in seconds (default: 5)
  - **Returns**: Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
  - **Raises**: RuntimeError on timeout or network errors

### Net::NTP::Packet

Internal class representing NTP protocol packets. Not intended for direct use.

#### Instance Methods

- `transmit_time` - Get transmit timestamp as Unix time
- `receive_time` - Get receive timestamp as Unix time
- `reference_time` - Get reference timestamp as Unix time

## NTP Protocol Details

### Timestamp Format

NTP uses 64-bit timestamps:
- Upper 32 bits: Seconds since NTP epoch (1900-01-01 00:00:00)
- Lower 32 bits: Fractional seconds (1/2^32 of a second)

### Epoch Conversion

- NTP epoch: 1900-01-01 00:00:00
- Unix epoch: 1970-01-01 00:00:00
- Offset: 2,208,988,800 seconds (70 years + 17 leap days)

### Packet Structure

NTP request/response packets are 48 bytes:
- Byte 0: Leap indicator, version, mode
- Bytes 1-3: Stratum, poll interval, precision
- Bytes 4-47: Timestamps and additional fields

## Implementation Notes

### Pure Ruby

This implementation is entirely in Ruby without C extensions, making it portable across all PicoRuby platforms.

### Binary Packing

Since PicoRuby doesn't have `Array#pack` or `String#unpack`, this gem implements manual binary packing/unpacking for NTP packets.

### Timeout Mechanism

The timeout is implemented using a simple polling loop with `Time.now.to_i` checks. This works on embedded systems without RTC by comparing relative time differences.

### Platform Support

- POSIX platforms (Linux, macOS): Full support
- RP2040/RP2350: Full support with LwIP stack
- Systems without RTC: Supported (transmit timestamp will be 0)

## Examples

See the [examples](examples/) directory for more usage examples.

## Testing

Tests are located in `test/ntp_test.rb`. Some tests require network connectivity and are commented out by default.

To run tests:

```bash
rake test
```

## Compatibility with CRuby

This implementation provides basic NTP client functionality compatible with common usage patterns:

- Basic time query: `Net::NTP.get(host, port, timeout)`
- Returns Unix timestamp (Integer)
- Similar error handling (raises on timeout/errors)

Differences from CRuby's net-ntp:
- Simplified API (no advanced NTP features)
- No subsecond precision in returned timestamps
- Manual binary packing instead of Array#pack

## License

MIT License - see LICENSE file for details

## Related Gems

- `picoruby-socket` - Socket implementation (required dependency)
- `picoruby-net-http` - HTTP client implementation

## References

- [RFC 5905 - Network Time Protocol Version 4](https://tools.ietf.org/html/rfc5905)
- [NTP.org - Network Time Protocol Project](https://www.ntp.org/)
