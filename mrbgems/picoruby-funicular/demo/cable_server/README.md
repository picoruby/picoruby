# ActionCable Test Server

A simple ActionCable-compatible WebSocket server for testing Funicular::Cable.

## Setup

Install dependencies:

```bash
bundle install
```

## Running the Server

From the PicoRuby root directory:

```bash
rake wasm:cable_server
```

Or directly from this directory:

```bash
ruby server.rb
```

The server will start on `ws://localhost:9292/cable`

## Features

- **Welcome Message**: Sends a welcome message on connection
- **Subscribe/Unsubscribe**: Handles subscription management
- **Message Broadcasting**: Broadcasts messages to all connected clients
- **Ping/Pong**: Automatic keepalive every 3 seconds

## Testing

You can test the server with the demo HTML file in `../www/test_cable.html`
