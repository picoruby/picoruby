# picoruby-net-websocket

WebSocket client and server for PicoRuby.

This is a pure Ruby implementation of WebSocket (RFC 6455) client and server for PicoRuby, designed for real-time communication.

## Features

- WebSocket client (RFC 6455)
- WebSocket server (RFC 6455, ws:// only)
- HTTP handshake with Sec-WebSocket-Key
- Text and binary frames
- Ping/Pong frames
- Close handshake
- Frame masking (client-to-server)
- Fragmented messages support
- Uses TCPSocket and SSLSocket from picoruby-socket

## Installation

The gem is part of the `networking` gembox, which the default build
configs already include:

```ruby
conf.gembox "networking"
```

To add only this gem, write:

```ruby
conf.gem core: 'picoruby-net-websocket'
```

The gem declares its dependencies itself: `picoruby-socket`,
`picoruby-base64`, `picoruby-rng`, `picoruby-mbedtls` and a `pack`
implementation (`mruby-pack` on PicoRuby, `picoruby-pack` on FemtoRuby).

## Usage

### Simple Connection

```ruby
require 'net/websocket'

# Connect to WebSocket server
ws = Net::WebSocket::Client.new("ws://echo.websocket.org")
ws.connect

# Send a text message
ws.send_text("Hello, WebSocket!")

# Receive a message
message = ws.receive
puts message

# Close connection
ws.close
```

### With Block

```ruby
require 'net/websocket'

Net::WebSocket::Client.connect("ws://echo.websocket.org") do |ws|
  ws.send_text("Hello!")
  response = ws.receive
  puts response
end
# Automatically closes
```

### Send and Receive

```ruby
ws = Net::WebSocket::Client.new("ws://example.com/socket")
ws.connect

# Send text
ws.send_text("Hello")

# Send binary data
binary_data = "\x00\x01\x02\x03"
ws.send_binary(binary_data)

# Receive message (blocking)
message = ws.receive
puts message

# Receive with timeout
message = ws.receive(timeout: 5)
if message
  puts message
else
  puts "Timeout"
end

ws.close
```

### Ping/Pong

```ruby
ws = Net::WebSocket::Client.new("ws://example.com")
ws.connect

# Send ping
ws.ping("optional payload")

# Pong is automatically sent when ping is received

ws.close
```

### Custom Headers

```ruby
ws = Net::WebSocket::Client.new("ws://example.com/socket")

# Add custom headers
ws.add_header("Authorization", "Bearer token123")
ws.add_header("X-Custom-Header", "value")

ws.connect
```

### Secure WebSocket (WSS)

```ruby
require 'net/websocket'

# Connect to wss:// (secure WebSocket)
ws = Net::WebSocket::Client.new("wss://secure.example.com/socket")

# Optional: Configure custom SSL context
ctx = SSLContext.new
ctx.verify_mode = SSLContext::VERIFY_PEER
ctx.ca_file = "/etc/ssl/certs/ca-certificates.crt"  # On POSIX systems
ws.ssl_context = ctx

ws.connect  # Performs SSL handshake and WebSocket handshake

ws.send_text("Secure message")
response = ws.receive

ws.close
```

### WSS with Self-Signed Certificate

```ruby
# Disable certificate verification (not recommended for production)
ws = Net::WebSocket::Client.new("wss://localhost:8443/socket")

ctx = SSLContext.new
ctx.verify_mode = SSLContext::VERIFY_NONE  # Disable verification
ws.ssl_context = ctx

ws.connect
```

### Server

The server accepts plain `ws://` connections. It does not terminate TLS.

```ruby
require 'net/websocket'

server = Net::WebSocket::Server.new(host: '0.0.0.0', port: 8080)
server.start

server.accept_loop do |conn|
  begin
    while (message = conn.receive(timeout: 60))
      conn.send(message)  # Echo the message back
    end
  rescue Net::WebSocket::ConnectionClosed
    # The client closed the connection
  ensure
    conn.close unless conn.closed?
  end
end
```

See `example/picoruby_server.rb` for a complete server.

## API Reference

### Net::WebSocket::Client

#### Class Methods

- `Net::WebSocket::Client.new(url)` - Create new WebSocket client
- `Net::WebSocket::Client.connect(url) { |ws| ... }` - Connect with block

#### Instance Methods

- `connect` - Perform WebSocket handshake (and SSL handshake if wss://)
- `connected?` - Check connection status
- `send_text(text)` - Send text message
- `send_binary(data)` - Send binary message
- `send(data, type: :text)` - Send message with type
- `receive(timeout: nil)` - Receive next message (blocking)
- `ping(payload = "")` - Send ping frame
- `close(code = 1000, reason = "")` - Close connection
- `add_header(name, value)` - Add custom HTTP header
- `ssl_context=(ctx)` - Set custom SSLContext for wss:// connections

### Net::WebSocket::Server

#### Class Methods

- `Net::WebSocket::Server.new(host: '0.0.0.0', port: 8080)` - Create new WebSocket server

#### Instance Methods

- `start` - Open the listening TCP socket
- `accept` - Wait for one client and return a `Connection` after the handshake
- `accept_loop { |conn| ... }` - Call the block for each accepted connection
- `close` - Close the listening socket

### Net::WebSocket::Server::Connection

- `send_text(text)` - Send text message
- `send_binary(data)` - Send binary message
- `send(data, type: :text)` - Send message with type
- `receive(timeout: nil)` - Receive next message (blocking); `timeout` is in seconds, nil on timeout
- `ping(payload = "")` - Send ping frame
- `close(code = 1000, reason = "")` - Close connection
- `closed?` - Check whether the socket is closed

## Supported Features

- ✅ Client-side WebSocket
- ✅ HTTP handshake with Sec-WebSocket-Key
- ✅ Text frames (opcode 0x1)
- ✅ Binary frames (opcode 0x2)
- ✅ Close frames (opcode 0x8)
- ✅ Ping frames (opcode 0x9)
- ✅ Pong frames (opcode 0xA)
- ✅ Frame masking
- ✅ Fragmented messages
- ✅ Custom headers
- ✅ SSL/TLS (wss://) supported via SSLSocket
- ✅ Server-side WebSocket (ws:// only, no TLS)
- ⚠️ Extensions not supported

## Example: Real-time Chat

```ruby
require 'net/websocket'

ws = Net::WebSocket::Client.new("ws://chat.example.com/socket")
ws.add_header("Authorization", "Bearer mytoken")
ws.connect

# Send join message
ws.send_text('{"action":"join","room":"lobby"}')

# Listen for messages
loop do
  message = ws.receive(timeout: 30)
  if message
    puts "Received: #{message}"
  else
    # Send ping to keep alive
    ws.ping
  end
end
```

## Example: IoT Device

```ruby
require 'net/websocket'

def read_sensor_data
  {
    temperature: 25.5,
    humidity: 60.0,
    timestamp: Time.now.to_i
  }
end

ws = Net::WebSocket::Client.new("ws://iot.example.com/device/sensor001")
ws.connect

# Send sensor data every 10 seconds
loop do
  data = read_sensor_data
  json = data.to_s  # Simple stringification
  ws.send_text(json)

  sleep 10
end
```

## Protocol Details

### WebSocket Frame Format

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               | Masking-key, if MASK set to 1 |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
```

### Opcodes

- 0x0: Continuation frame
- 0x1: Text frame
- 0x2: Binary frame
- 0x8: Close frame
- 0x9: Ping frame
- 0xA: Pong frame

### Close Codes

- 1000: Normal closure
- 1001: Going away
- 1002: Protocol error
- 1003: Unsupported data
- 1006: Abnormal closure

## License

MIT
