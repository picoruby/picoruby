#!/usr/bin/env ruby

# CRuby WebSocket client for testing PicoRuby server
#
# Usage:
#   Terminal 1: picoruby picoruby_server.rb
#   Terminal 2: ruby cruby_client.rb

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'websocket-client-simple'
end

SERVER_URL = "ws://localhost:8080"

def run_tests
  passed = 0
  failed = 0

  puts "Connecting to PicoRuby WebSocket server at #{SERVER_URL}..."

  ws = WebSocket::Client::Simple.connect(SERVER_URL)

  # Wait for connection
  sleep 1

  unless ws.open?
    puts "FAIL: Failed to connect"
    return [0, 1]
  end

  puts "PASS: Connected to server"
  passed += 1

  received_messages = []
  closing = false

  ws.on :message do |msg|
    received_messages << msg.data
    puts "Received: #{msg.data.inspect}"
  end

  ws.on :close do |e|
    puts "Connection closed: #{e.inspect}" unless closing
  end

  ws.on :error do |e|
    # Ignore errors during close (stream closed in another thread)
    puts "Error: #{e.inspect}" unless closing
  end

  # Test 1: Text echo
  begin
    test_message = "Hello from CRuby!"
    ws.send(test_message)
    puts "Sent: #{test_message.inspect}"

    sleep 0.5
    if received_messages.include?(test_message)
      puts "PASS: Echo test"
      passed += 1
      received_messages.clear
    else
      puts "FAIL: Echo test - no response"
      failed += 1
    end
  rescue => e
    puts "FAIL: Echo test raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test 2: Binary echo
  begin
    binary_data = "\x00\x01\x02\x03\x04"
    ws.send(binary_data, type: :binary)
    puts "Sent binary: #{binary_data.length} bytes"

    sleep 0.5
    if received_messages.any? { |m| m == binary_data }
      puts "PASS: Binary echo test"
      passed += 1
      received_messages.clear
    else
      puts "FAIL: Binary echo test"
      failed += 1
    end
  rescue => e
    puts "FAIL: Binary test raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test 3: Multiple messages
  begin
    messages = ["First", "Second", "Third"]
    messages.each do |msg|
      ws.send(msg)
    end

    sleep 1
    if messages.all? { |m| received_messages.include?(m) }
      puts "PASS: Multiple messages test"
      passed += 1
      received_messages.clear
    else
      puts "FAIL: Multiple messages test"
      failed += 1
    end
  rescue => e
    puts "FAIL: Multiple messages test: #{e.message}"
    failed += 1
  end

  # Test 4: Empty message
  begin
    ws.send("")
    sleep 0.5
    if received_messages.include?("")
      puts "PASS: Empty message test"
      passed += 1
      received_messages.clear
    else
      puts "FAIL: Empty message test"
      failed += 1
    end
  rescue => e
    puts "FAIL: Empty message test: #{e.message}"
    failed += 1
  end

  # Test 5: Long message
  begin
    long_message = "x" * 1000
    ws.send(long_message)
    sleep 0.5
    if received_messages.include?(long_message)
      puts "PASS: Long message test (1000 bytes)"
      passed += 1
      received_messages.clear
    else
      puts "FAIL: Long message test"
      failed += 1
    end
  rescue => e
    puts "FAIL: Long message test: #{e.message}"
    failed += 1
  end

  puts ""
  puts "="*50
  puts "Test Results: #{passed} passed, #{failed} failed"
  puts "="*50

  closing = true
  ws.close
  sleep 0.5

  [passed, failed]
end

passed, failed = run_tests
exit(failed > 0 ? 1 : 0)
