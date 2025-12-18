# PicoRuby Socket Examples

This directory contains example programs demonstrating how to use picoruby-socket.

## TCP Examples

### tcp_client.rb
Simple TCP client that connects to a server and sends a message.

```bash
ruby tcp_client.rb [host]
```

### tcp_server.rb
TCP server that accepts connections and echoes received data.

```bash
ruby tcp_server.rb
```

### tcp_server_simple.rb
Minimal TCP server example.

```bash
ruby tcp_server_simple.rb
```

## UDP Examples

### udp_client.rb
UDP client that sends a message to a server.

```bash
ruby udp_client.rb [host]
```

### udp_server.rb
UDP server that receives and displays messages.

```bash
ruby udp_server.rb
```

## TLS/SSL Examples

### tls_client_simple.rb
Minimal TLS client example - the easiest way to get started with TLS.

**Usage:**
```bash
ruby tls_client_simple.rb
```

**Features:**
- Simple HTTPS connection
- Certificate verification disabled (for testing)
- Minimal code for quick prototyping

### tls_client.rb
Standard TLS client with proper error handling and certificate verification.

**Usage:**
```bash
ruby tls_client.rb [hostname]
```

**Features:**
- Certificate verification enabled
- Complete HTTPS request/response handling
- Production-ready example

### tls_client_embedded.rb
TLS client optimized for embedded platforms (RP2040, ESP32).

**Usage:**
```bash
ruby tls_client_embedded.rb
```

**Features:**
- Shows how to use CA certificates on embedded systems
- Memory-efficient certificate loading
- Platform-specific considerations

## TLS Certificate Verification

### Development/Testing
For quick testing, you can disable certificate verification:

```ruby
ctx = SSLContext.new
ctx.verify_mode = SSLContext::VERIFY_NONE
```

### Production (POSIX platforms)
On Linux/macOS, use system CA certificates:

```ruby
ctx = SSLContext.new
ctx.verify_mode = SSLContext::VERIFY_PEER
# System CA certificates are loaded automatically
```

Or specify a CA file:

```ruby
ctx = SSLContext.new
ctx.ca_file = "/path/to/ca-bundle.crt"
ctx.verify_mode = SSLContext::VERIFY_PEER
```

### Production (Embedded platforms)
On embedded platforms (RP2040, ESP32), load CA certificate from memory:

```ruby
# Define CA certificate as a string constant
CA_CERT = <<~CERT
  -----BEGIN CERTIFICATE-----
  MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ
  ...
  -----END CERTIFICATE-----
CERT

# Get address and size
ca_cert_addr = CA_CERT.object_id
ca_cert_size = CA_CERT.bytesize

# Configure SSL context
ctx = SSLContext.new
ctx.set_ca_cert(ca_cert_addr, ca_cert_size)
ctx.verify_mode = SSLContext::VERIFY_PEER
```

## API Usage

### Basic TLS Connection

```ruby
require 'socket'

# Create SSL context
ctx = SSLContext.new
ctx.verify_mode = SSLContext::VERIFY_PEER

# Create TCP socket
tcp = TCPSocket.new('example.com', 443)

# Wrap with SSL
# IMPORTANT: SSLSocket.new extracts hostname and port from tcp,
# then closes tcp and creates a new TCP connection internally.
# This is for API compatibility with CRuby.
ssl = SSLSocket.new(tcp, ctx)
ssl.connect

# Use the SSL socket
ssl.write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
response = ssl.read(1024)

# Close
ssl.close
```

### Using SSLSocket.open

```ruby
ctx = SSLContext.new
tcp = TCPSocket.new('example.com', 443)
ssl = SSLSocket.open(tcp, ctx)  # Connects automatically
```

## Implementation Notes

### How SSLSocket Works

Unlike CRuby's OpenSSL::SSL::SSLSocket, PicoRuby's SSLSocket does not wrap an existing TCP connection. Instead:

1. `SSLSocket.new(tcp, ctx)` extracts the hostname and port from the TCPSocket
2. The original TCPSocket is immediately closed
3. When `ssl.connect` is called, a new TCP connection is created internally
4. The SSL/TLS handshake is performed on the new connection

This approach:
- ✅ Provides API compatibility with CRuby
- ✅ Simplifies implementation on embedded platforms
- ✅ Avoids resource leaks
- ⚠️ Does not reuse the existing TCP connection

### Why This Design?

On embedded platforms (RP2040, ESP32), upgrading an existing TCP connection to TLS is complex due to:
- LwIP's altcp architecture
- Memory constraints
- Platform-specific TLS libraries

The current design balances API compatibility with implementation simplicity.

### Known Limitations

**Use `write` instead of `puts` for SSL sockets:**

The `puts` method internally calls `write` twice (once for the string, once for the newline), which can cause SSL_ERROR_SYSCALL errors with small writes. Use `write` with explicit `\r\n` line endings instead:

```ruby
# Don't use puts with SSL
# ssl.puts "GET / HTTP/1.1"  # May cause SSL_ERROR_SYSCALL

# Use write instead
ssl.write("GET / HTTP/1.1\r\n")
```

## Platform Notes

### POSIX (Linux, macOS)
- Uses OpenSSL
- System CA certificates available
- `ca_file` parameter supported

### RP2040
- Uses mbedTLS via LwIP altcp_tls
- Requires CA certificate in memory
- Use `set_ca_cert(addr, size)` method

### ESP32
- SSL support available (implementation TBD)
- Platform-specific configuration may be required

## See Also

- [picoruby-net-http](../picoruby-net-http) - Higher-level HTTP/HTTPS client
- [CRuby OpenSSL::SSL documentation](https://ruby-doc.org/stdlib-3.0.0/libdoc/openssl/rdoc/OpenSSL/SSL.html)
