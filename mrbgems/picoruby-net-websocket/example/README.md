# WebSocket Interoperability Test

This directory contains test scripts to verify WebSocket communication between CRuby and PicoRuby.

## Prerequisites

Install the required CRuby gems:

```bash
gem install em-websocket
```

## Running the Tests

### Terminal 1: Start CRuby WebSocket Server

```bash
cd mrbgems/picoruby-net-websocket/example
ruby cruby_server.rb
```

You should see:
```
Starting CRuby WebSocket server on ws://0.0.0.0:8080
Press Ctrl+C to stop
Server ready!
```

### Terminal 2: Run PicoRuby Client

```bash
cd mrbgems/picoruby-net-websocket/example
picoruby picoruby_client.rb
```

## Test Cases

The test suite covers:

1. **Connection Test** - Verify WebSocket handshake and connection
2. **Text Echo Test** - Send and receive text messages
3. **Binary Echo Test** - Send and receive binary data
4. **Generic Send Test** - Test the generic `send()` method with type parameter
5. **Multiple Messages** - Verify multiple sequential messages
6. **Ping/Pong Test** - Test WebSocket ping frames
7. **Empty Message Test** - Handle empty payloads
8. **Long Message Test** - Test with 1000-byte message
9. **Graceful Close Test** - Proper connection closure

## Expected Output

When all tests pass, you should see output similar to:

```
Connecting to WebSocket server at ws://localhost:8080...
PASS: Connected to server
Sent: "Hello from PicoRuby!"
PASS: Echo test - received: "Hello from PicoRuby!"
Sent binary: 5 bytes
PASS: Binary echo test
...
==================================================
Test Results: 8 passed, 0 failed
==================================================
```

## Server Features

The CRuby server acts as an echo server and also supports special commands:

- `PING_TEST` - Server sends a ping frame
- `CLOSE_TEST` - Server initiates connection close
- `BINARY_TEST` - Server sends binary test data

## Troubleshooting

### Connection Refused

If you see "Connection refused", make sure:
- The CRuby server is running in Terminal 1
- Port 8080 is not blocked by firewall
- No other service is using port 8080

### Timeout Errors

If messages timeout:
- Check network connectivity
- Increase timeout values in the client script
- Check server logs for errors

### EventMachine Issues

If EventMachine fails to install:
- On macOS: `xcode-select --install`
- On Linux: Install build tools (`build-essential` on Debian/Ubuntu)
