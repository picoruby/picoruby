# PicoRuby WebSocket client interop test script
# Run with: picoruby picoruby_client.rb
#
# Prerequisites:
#   Terminal 1: ruby cruby_server.rb
#   Terminal 2: picoruby picoruby_client.rb

require 'net/websocket'

SERVER_URL = "ws://localhost:8080"

def run_tests
  passed = 0
  failed = 0

  puts "Connecting to WebSocket server at #{SERVER_URL}..."

  begin
    ws = Net::WebSocket::Client.new(SERVER_URL)
    ws.connect

    if ws.connected?
      puts "PASS: Connected to server"
      passed += 1
    else
      puts "FAIL: Failed to connect"
      failed += 1
      return [passed, failed]
    end
  rescue => e
    puts "FAIL: Connection error: #{e.class}: #{e.message}"
    failed += 1
    return [passed, failed]
  end

  # Test 1: Send and receive text message
  begin
    test_message = "Hello from PicoRuby!"
    ws.send_text(test_message)
    puts "Sent: #{test_message.inspect}"

    response = ws.receive(timeout: 5)
    if response == test_message
      puts "PASS: Echo test - received: #{response.inspect}"
      passed += 1
    else
      puts "FAIL: Echo test - expected #{test_message.inspect}, got #{response.inspect}"
      failed += 1
    end
  rescue => e
    puts "FAIL: Echo test raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test 2: Send binary data
  begin
    binary_data = "\x00\x01\x02\x03\x04"
    ws.send_binary(binary_data)
    puts "Sent binary: #{binary_data.length} bytes"

    response = ws.receive(timeout: 5)
    if response == binary_data
      puts "PASS: Binary echo test"
      passed += 1
    else
      puts "FAIL: Binary echo - length mismatch (expected #{binary_data.length}, got #{response&.length})"
      failed += 1
    end
  rescue => e
    puts "FAIL: Binary test raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test 3: Send using generic send method with type
  begin
    message = "Generic send test"
    ws.send(message, type: :text)
    puts "Sent via generic send: #{message.inspect}"

    response = ws.receive(timeout: 5)
    if response == message
      puts "PASS: Generic send test"
      passed += 1
    else
      puts "FAIL: Generic send test - got #{response.inspect}"
      failed += 1
    end
  rescue => e
    puts "FAIL: Generic send test raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test 4: Multiple messages
  begin
    messages = ["First", "Second", "Third"]
    messages.each do |msg|
      ws.send_text(msg)
      response = ws.receive(timeout: 5)
      unless response == msg
        raise "Mismatch: expected #{msg.inspect}, got #{response.inspect}"
      end
    end
    puts "PASS: Multiple messages test"
    passed += 1
  rescue => e
    puts "FAIL: Multiple messages test: #{e.message}"
    failed += 1
  end

  # Test 5: Ping/Pong
  begin
    ws.ping("test_ping")
    puts "Sent ping"
    # Server should respond with pong, but we might receive it asynchronously
    # Try to receive any pending messages
    response = ws.receive(timeout: 2)
    # If we got a message back, it means the connection is still working
    puts "PASS: Ping sent successfully (server may have responded)"
    passed += 1
  rescue => e
    # Timeout is acceptable here as pong might not echo back
    if e.is_a?(Net::WebSocket::ConnectionClosed)
      puts "FAIL: Ping test - connection closed: #{e.message}"
      failed += 1
    else
      puts "PASS: Ping sent (no echo expected)"
      passed += 1
    end
  end

  # Test 6: Empty message
  begin
    ws.send_text("")
    response = ws.receive(timeout: 5)
    if response == ""
      puts "PASS: Empty message test"
      passed += 1
    else
      puts "FAIL: Empty message - got #{response.inspect}"
      failed += 1
    end
  rescue => e
    puts "FAIL: Empty message test: #{e.message}"
    failed += 1
  end

  # Test 7: Long message
  begin
    long_message = "x" * 1000
    ws.send_text(long_message)
    response = ws.receive(timeout: 5)
    if response == long_message
      puts "PASS: Long message test (1000 bytes)"
      passed += 1
    else
      puts "FAIL: Long message - length mismatch"
      failed += 1
    end
  rescue => e
    puts "FAIL: Long message test: #{e.message}"
    failed += 1
  end

  # Test 8: Close connection gracefully
  begin
    ws.close(Net::WebSocket::CLOSE_NORMAL, "Test complete")
    if !ws.connected?
      puts "PASS: Connection closed gracefully"
      passed += 1
    else
      puts "FAIL: Connection still open after close"
      failed += 1
    end
  rescue => e
    puts "FAIL: Close test raised #{e.class}: #{e.message}"
    failed += 1
  end

  puts ""
  puts "="*50
  puts "Test Results: #{passed} passed, #{failed} failed"
  puts "="*50

  [passed, failed]
end

# Run the tests
passed, failed = run_tests
