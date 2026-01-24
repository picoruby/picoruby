# picoruby-socket

CRuby-compatible Socket implementation for PicoRuby.

## Overview

`picoruby-socket` provides standard Ruby socket classes for PicoRuby, enabling network communication with an API compatible with CRuby's socket library.

## Features

- ✅ **CRuby-compatible API**: Use familiar Ruby socket methods
- ✅ **TCPSocket**: TCP client connections
- ✅ **UDPSocket**: UDP communication with send/receive/bind
- ✅ **TCPServer**: TCP server with accept and backlog support
- ✅ **SSLSocket**: Secure TLS/SSL connections
- ✅ **Certificate Verification**: VERIFY_PEER support on all platforms
- ✅ **IO-compatible**: Works with code expecting IO-like objects
- ✅ **POSIX support**: Native sockets on Linux/macOS/Unix with OpenSSL
- ✅ **RP2040 support**: LwIP stack with MbedTLS for microcontrollers
- ✅ **ROM Certificates**: Efficient certificate loading from flash memory
- ✅ **Dual VM support**: Works with both mruby and mruby/c

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

### SSL/TLS Connections with Certificate Verification

#### On POSIX Platforms (Linux/macOS)

Use file paths to CA certificates:

```ruby
require 'socket'

# Create SSL context
ctx = SSLContext.new
ctx.ca_file = "/etc/ssl/certs/ca-certificates.crt"  # Path to CA bundle
ctx.verify_mode = SSLContext::VERIFY_PEER            # Enable verification (default)

# Connect to HTTPS server
tcp = TCPSocket.new('example.com', 443)
ssl = SSLSocket.new(tcp, ctx)
ssl.connect

# Use SSL socket like regular socket
ssl.write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
response = ssl.read(1024)

ssl.close
```

#### On RP2040/Microcontrollers

Use ROM-based certificates with physical memory addresses:

```ruby
require 'socket'
require 'fat'  # picoruby-filesystem-fat

# Open CA certificate file from flash
file = FAT::File.new("/flash/ca-bundle.crt", "r")
addr = file.physical_address  # Get ROM address
size = file.size
file.close

# Create SSL context with ROM certificate
ctx = SSLContext.new
ctx.set_ca(addr, size)  # Load certificate from ROM address
ctx.verify_mode = SSLContext::VERIFY_PEER

# Connect to HTTPS server
tcp = TCPSocket.new('example.com', 443)
ssl = SSLSocket.new(tcp, ctx)
ssl.connect

# Use SSL socket
ssl.write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
response = ssl.read(1024)

ssl.close
```

**Note**: On RP2040 and ESP32, `ctx.ca_file=` is not supported. Use `ctx.set_ca(addr, size)` instead to load certificates directly from ROM/flash memory.

#### Disable Certificate Verification (Not Recommended)

```ruby
ctx = SSLContext.new
ctx.verify_mode = SSLContext::VERIFY_NONE  # Disable verification

# Rest of the code...
```

**Security Warning**: Disabling certificate verification makes your connection vulnerable to man-in-the-middle attacks. Only use `VERIFY_NONE` for testing purposes.

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

### SSLContext

#### Class Methods

- `SSLContext.new()` - Create new SSL context

#### Constants

- `SSLContext::VERIFY_NONE` - Disable certificate verification
- `SSLContext::VERIFY_PEER` - Enable certificate verification (default)

#### Instance Methods

- `ca_file=(path)` - Set CA certificate file path (POSIX only)
- `set_ca(addr, size)` - Set CA certificate from ROM address (RP2040 only)
- `verify_mode=(mode)` - Set verification mode (VERIFY_NONE or VERIFY_PEER)
- `verify_mode` - Get current verification mode

### SSLSocket

#### Class Methods

- `SSLSocket.new(tcp_socket, ssl_context)` - Create SSL socket wrapping TCP context

#### Instance Methods

- `connect` - Perform SSL/TLS handshake
- `write(data)` - Send encrypted data
- `read(maxlen = nil)` - Read encrypted data
- `close` - Close SSL connection
- `closed?` - Check if closed
- `remote_host` - Get remote hostname
- `remote_port` - Get remote port

All IO-compatible methods from BasicSocket are also available.

## Implementation Status

### Phase 1 ✅ Completed
- TCPSocket with POSIX implementation
- Basic read/write/close operations
- IO-compatible methods
- Dual VM bindings (mruby and mruby/c)

### Phase 2 ✅ Completed
- UDPSocket implementation
- UDP send/receive/sendto operations
- Bind functionality

### Phase 3 ✅ Completed
- TCPServer implementation
- Accept connections
- Server backlog support

### Phase 4 ✅ Completed
- Net::HTTP implementation (separate gem: picoruby-net-http)
- GET, POST, PUT, DELETE support
- Regex-free implementation for VM compatibility

### Phase 5 ✅ Completed
- SSLSocket with OpenSSL (POSIX) and MbedTLS (RP2040)
- HTTPS support via Net::HTTP
- Certificate verification with VERIFY_PEER
- SNI (Server Name Indication) support
- ROM-based certificates for microcontrollers

### Phase 6 ✅ Completed
- LwIP implementation for RP2040
- Raspberry Pi Pico support
- Callback-based async architecture
- Platform abstraction layer

## Platform Support

### POSIX Platforms
- ✅ Linux
- ✅ macOS
- ✅ Unix-like systems
- Uses native POSIX sockets
- OpenSSL for SSL/TLS
- File-based CA certificates

### Embedded Platforms
- ✅ Raspberry Pi Pico (RP2040/RP2350)
- Uses LwIP TCP/IP stack
- MbedTLS for SSL/TLS
- ROM-based CA certificates via `FAT::File#physical_address`
- Supports both mruby and mruby/c VMs

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
