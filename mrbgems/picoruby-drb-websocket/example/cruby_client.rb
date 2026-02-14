#!/usr/bin/env ruby

# CRuby DRb WebSocket Client Example
#
# ⚠️ WARNING: This example is NOT COMPATIBLE with PicoRuby servers!
#
# The CRuby drb-websocket gem uses a different protocol (36-byte UUID prefix)
# that is incompatible with PicoRuby's simple protocol implementation.
#
# This file is kept for reference only. It will NOT work with PicoRuby servers.
#
# Usage (for CRuby servers only):
#   Terminal 1: ruby cruby_server.rb
#   Terminal 2: ruby cruby_client.rb [URI]
#   Default URI: ws://127.0.0.1:8080

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'drb'
  gem 'drb-websocket'
end

# Server URI
uri = ARGV[0] || 'ws://127.0.0.1:8080'

puts "Connecting to DRb WebSocket server..."
puts "URI: #{uri}"
puts ""

# Create reference to remote object
service = DRbObject.new_with_uri(uri)

begin
  # Test different methods based on server type
  puts "=== Basic Tests ==="
  puts ""

  # Try PicoRuby server methods
  begin
    puts "Test: hello (PicoRuby server)"
    result = service.hello('CRuby Client')
    puts "  Result: #{result}"
    puts ""
  rescue NoMethodError
    puts "  (Method not available - not a PicoRuby server)"
    puts ""
  end

  # Try CRuby server methods
  begin
    puts "Test: greet (CRuby server)"
    result = service.greet('PicoRuby')
    puts "  Result: #{result}"
    puts ""
  rescue NoMethodError
    puts "  (Method not available - not a CRuby server)"
    puts ""
  end

  # Common tests
  puts "Test: Echo"
  message = "Hello from CRuby!"
  echo = service.echo(message)
  puts "  Sent: #{message}"
  puts "  Received: #{echo}"
  puts "  Match: #{message == echo}"
  puts ""

  puts "Test: Counter"
  puts "  Initial counter: #{service.counter}"
  service.increment
  puts "  After increment: #{service.counter}"
  service.increment
  puts "  After increment: #{service.counter}"
  puts ""

  # Try data types test
  begin
    puts "Test: Data Types"
    if service.respond_to?(:data_types_test)
      data = service.data_types_test
    elsif service.respond_to?(:test_data)
      data = service.test_data
    else
      raise NoMethodError
    end

    puts "  Received data:"
    data.each do |key, value|
      puts "    #{key}: #{value.inspect} (#{value.class})"
    end
    puts ""
  rescue NoMethodError
    puts "  (Data types test not available)"
    puts ""
  end

  puts "All tests completed successfully!"

rescue DRb::DRbConnError => e
  puts "Connection error: #{e.message}"
  puts "Make sure the server is running at #{uri}"
  exit 1
rescue => e
  puts "Error: #{e.class}: #{e.message}"
  puts e.backtrace.first(5)
  exit 1
end
