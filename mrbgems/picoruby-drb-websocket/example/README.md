# DRb over WebSocket Examples

This directory contains examples demonstrating DRb communication over WebSocket.

## Overview

DRb (Distributed Ruby) over WebSocket enables seamless remote method invocation between:
- **Microcontrollers** running PicoRuby
- **Browsers** running PicoRuby WASM

**Important**: This implementation uses a simple PicoRuby-specific protocol and is **NOT compatible** with CRuby's `drb-websocket` gem. It is designed specifically for PicoRuby-to-PicoRuby communication.

## Prerequisites

### For PicoRuby
- PicoRuby with `picoruby-drb-websocket` gem
- Build configuration including networking support

### For WASM/Browser
- PicoRuby WASM build with `picoruby-drb-websocket` gem
- Modern web browser with WebSocket support

## Quick Start

### PicoRuby Server + PicoRuby Client

Terminal 1 (Server):
```bash
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb server
```

Terminal 2 (Client):
```bash
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb client
```

Or with custom URI:
```bash
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb client ws://192.168.1.100:8080
```

### PicoRuby Server + Browser Client (WASM)

Terminal 1 (Server):
```bash
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb server
```

Browser:
```
Open wasm/browser_client.html in a web browser
```

## Example Files

### `picoruby_interop.rb`

Unified PicoRuby DRb server/client example.

**Server mode** - Exposes a SensorService with methods:
- `hello(name)` - Greeting
- `temperature` - Simulated sensor reading
- `increment/counter` - Stateful counter
- `echo(message)` - Echo test
- `current_time` - Server time
- `data_types_test` - Test various Ruby data types

**Client mode** - Connects to a server and runs comprehensive tests.

Usage:
```bash
# Start server
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb server

# Run client (connects to ws://localhost:8080 by default)
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb client

# Run client with custom URI
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb client ws://192.168.1.100:8080
```

### `cruby_server.rb` and `cruby_client.rb`

**⚠️ These files are NOT compatible with PicoRuby!**

These files demonstrate CRuby's `drb-websocket` gem usage, which uses a different protocol (36-byte UUID prefix). They are kept for reference only and cannot communicate with PicoRuby servers/clients.

### `wasm/browser_client.html`

HTML/JavaScript client demonstrating DRb communication from browser.

**Note**: This requires PicoRuby WASM build with `picoruby-drb-websocket` gem.
The HTML file contains a UI demo and implementation guidelines.

To use:
1. Build PicoRuby WASM with networking support
2. Serve the HTML file via HTTP server
3. Start a DRb WebSocket server
4. Open in browser and connect

## Interoperability Matrix

| Server | Client | Status |
|--------|--------|--------|
| PicoRuby | PicoRuby | ✅ Full support |
| PicoRuby | Browser (WASM) | ✅ Client only |
| CRuby (`drb-websocket`) | PicoRuby | ❌ Not compatible |
| PicoRuby | CRuby (`drb-websocket`) | ❌ Not compatible |

**Note**: PicoRuby uses a simple protocol (direct DRb messages in WebSocket frames), while CRuby's `drb-websocket` gem uses a complex protocol with 36-byte UUID prefixes.

## Protocol Details

### Transport
- WebSocket binary frames (opcode 0x2)
- Simple protocol: DRb messages directly in WebSocket frames
- **NOT compatible** with CRuby's `drb-websocket` gem

### Message Format
Each DRb message consists of:
1. 4-byte length header (big-endian)
2. Marshal-serialized data

Multiple messages are sent for each RPC:
- Request: ref, method_id, argc, args..., block
- Reply: success, result

### URI Scheme
- `ws://host:port` - WebSocket (unencrypted)
- `wss://host:port` - WebSocket Secure (TLS) - client support only

## Troubleshooting

### Connection Refused
- Ensure server is running and listening on correct port
- Check firewall settings
- Verify URI matches server configuration

### Protocol Version Mismatch
- This implementation is PicoRuby-specific
- Cannot connect to CRuby `drb-websocket` servers
- Use PicoRuby servers for both client and server

### Marshal Error
- Ensure compatible PicoRuby versions
- Some complex data types may not Marshal correctly
- Use simple data types (String, Integer, Array, Hash) for maximum compatibility

### Timeout
- Default timeout is 10 seconds for receive operations
- Network latency or slow operations may cause timeouts
- Check server logs for errors

## Development Tips

1. **Start Simple**: Begin with echo test to verify connectivity
2. **Test Data Types**: Use simple types (String, Integer, Array, Hash)
3. **Handle Errors**: Wrap remote calls in begin/rescue blocks
4. **Use Logging**: Add debug output to trace issues
5. **PicoRuby Only**: Remember this is PicoRuby-to-PicoRuby only

## Testing

Run the integration test from project root:
```bash
bash mrbgems/picoruby-drb-websocket/example/test_integration.sh
```

Or from the example directory:
```bash
cd mrbgems/picoruby-drb-websocket/example
bash test_integration.sh
```

This will start a server and run a client with comprehensive tests.

## Further Reading

- [DRb Documentation](https://ruby-doc.org/stdlib/libdoc/drb/rdoc/DRb.html)
- [WebSocket Protocol (RFC 6455)](https://tools.ietf.org/html/rfc6455)
- PicoRuby documentation
