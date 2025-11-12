# picoruby-socket

CRuby-compatible Socket implementation for PicoRuby.

## Overview

`picoruby-socket` provides standard Ruby socket classes for PicoRuby, enabling network communication with an API compatible with CRuby's socket library.

## Features

- ✅ **CRuby-compatible API**: Use familiar Ruby socket methods
- ✅ **TCPSocket**: TCP client connections
- ✅ **IO-compatible**: Works with code expecting IO-like objects
- ✅ **POSIX support**: Native socket support on Linux/macOS/Unix
- 🚧 **UDPSocket**: UDP communication (Phase 2)
- 🚧 **TCPServer**: TCP server functionality (Phase 3)
- 🚧 **SSL/TLS**: Secure connections via mbedTLS (Phase 5)
- 🚧 **LwIP support**: Embedded/microcontroller support (Phase 6)

## Installation

Add to your `build_config.rb`:

```ruby
conf.gem :core => 'picoruby-socket'
```

## Usage

### Basic TCP Connection

```ruby
require 'socket'

# Connect to a server
socket = TCPSocket.new('example.com', 80)

# Send data
socket.write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")

# Read response
response = socket.read(1024)
puts response

# Close connection
socket.close
```

### IO-Compatible Methods

```ruby
socket = TCPSocket.new('example.com', 80)

# Use IO-like methods
socket.puts("GET / HTTP/1.1")
socket.puts("Host: example.com")
socket.puts("")

line = socket.gets
puts line

socket.close
```

### Check Connection Status

```ruby
socket = TCPSocket.new('example.com', 80)

puts socket.remote_host  # => "example.com"
puts socket.remote_port  # => 80
puts socket.closed?      # => false

socket.close
puts socket.closed?      # => true
```

## API Reference

### TCPSocket

#### Class Methods

- `TCPSocket.new(host, port)` - Create new TCP connection
- `TCPSocket.open(host, port)` - Alias for `new`
- `TCPSocket.gethostbyname(host)` - Resolve hostname (simplified)

#### Instance Methods

- `write(data)` - Send data, returns bytes sent
- `read(maxlen = nil)` - Read data, returns string or nil on EOF
- `gets(sep = "\n")` - Read line
- `puts(*args)` - Write lines
- `print(*args)` - Write without newline
- `close` - Close connection
- `closed?` - Check if closed
- `remote_host` - Get remote hostname
- `remote_port` - Get remote port
- `peeraddr` - Get peer address info
- `send(data, flags = 0)` - Send with flags (flags ignored for now)
- `recv(maxlen, flags = 0)` - Receive with flags (flags ignored for now)

### BasicSocket

Base class for all socket types. Provides common socket and IO-compatible methods.

## Implementation Status

### Phase 1 ✅ (Current)
- TCPSocket with POSIX implementation
- Basic read/write/close operations
- IO-compatible methods
- mruby VM bindings

### Phase 2 🚧 (Planned)
- UDPSocket implementation
- UDP send/receive

### Phase 3 🚧 (Planned)
- TCPServer implementation
- Accept connections
- Server examples

### Phase 4 🚧 (Planned)
- Net::HTTP implementation (separate gem: picoruby-net-http)

### Phase 5 🚧 (Planned)
- SSLSocket with mbedTLS
- HTTPS support

### Phase 6 🚧 (Planned)
- LwIP implementation for embedded systems
- Raspberry Pi Pico support

## Platform Support

### Currently Supported
- ✅ Linux
- ✅ macOS
- ✅ Unix-like systems (POSIX)

### Planned Support
- 🚧 Raspberry Pi Pico (LwIP)
- 🚧 Other embedded platforms with LwIP

## Architecture

```
┌─────────────────────────────────────┐
│      Ruby Application Code          │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│   Ruby API (mrblib/)                │
│   - TCPSocket                       │
│   - BasicSocket                     │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│   mruby VM Bindings (src/mruby/)    │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│   C Implementation (ports/posix/)   │
│   - socket()                        │
│   - connect()                       │
│   - send() / recv()                 │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│   OS Network Stack                  │
│   (Linux/macOS kernel)              │
└─────────────────────────────────────┘
```

## Examples

See the [test](test/) directory for more examples.

## Testing

Tests are located in `test/tcp_socket_test.rb`. Some tests require network connectivity and are commented out by default.

To run tests:

```bash
rake test
```

## Compatibility with CRuby

This implementation aims for compatibility with CRuby's socket library, but with some limitations:

- ✅ Most common TCPSocket methods
- ✅ IO-compatible methods (read, write, gets, puts)
- ⚠️ `is_a?(IO)` returns false (duck typing approach)
- ⚠️ Some socket options not supported
- ❌ Unix domain sockets not supported
- ❌ Raw sockets not supported

## Contributing

Contributions are welcome! Please see the main PicoRuby repository for contribution guidelines.

## License

MIT License - see LICENSE file for details

## Related Gems

- `picoruby-net-http` - Net::HTTP implementation (Phase 4)
- `picoruby-mbedtls` - TLS/SSL support (Phase 5)
- `picoruby-net` - Legacy network implementation (deprecated)

## Design Documents

See [docs/socket-design-plan.md](../../docs/socket-design-plan.md) for detailed design information.
