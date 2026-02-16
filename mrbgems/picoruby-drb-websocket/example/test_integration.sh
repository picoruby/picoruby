#!/bin/bash

# Test script for DRb WebSocket
# Tests PicoRuby server with PicoRuby client

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$PROJECT_ROOT"

echo "=========================================="
echo "DRb WebSocket Integration Test"
echo "=========================================="
echo ""

# Start server in background
echo "Starting PicoRuby DRb WebSocket server..."
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/server_client.rb server &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Check if server is running
if ! ps -p $SERVER_PID > /dev/null; then
    echo "ERROR: Server failed to start"
    exit 1
fi

echo "Server started (PID: $SERVER_PID)"
echo ""

# Run client
echo "Running PicoRuby client..."
echo "=========================================="
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/server_client.rb client
CLIENT_EXIT=$?
echo "=========================================="
echo ""

# Cleanup
echo "Stopping server..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

if [ $CLIENT_EXIT -eq 0 ]; then
    echo ""
    echo "✅ Test PASSED"
    exit 0
else
    echo ""
    echo "❌ Test FAILED"
    exit 1
fi
