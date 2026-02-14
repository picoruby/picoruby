#!/usr/bin/env ruby

# CRuby DRb WebSocket Server Example
#
# ⚠️ WARNING: This example is NOT COMPATIBLE with PicoRuby clients!
#
# The CRuby drb-websocket gem uses a different protocol (36-byte UUID prefix)
# that is incompatible with PicoRuby's simple protocol implementation.
#
# This file is kept for reference only. It will NOT work with PicoRuby clients.
#
# Usage (for CRuby clients only):
#   Terminal 1: ruby cruby_server.rb
#   Terminal 2: ruby cruby_client.rb

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'drb'
  gem 'drb-websocket'
end

# Sample service object
class TestService
  def initialize
    @counter = 0
    @start_time = Time.now
  end

  def greet(name)
    "Greetings from CRuby, #{name}!"
  end

  def add(a, b)
    a + b
  end

  def multiply(a, b)
    a * b
  end

  def increment
    @counter += 1
  end

  def counter
    @counter
  end

  def uptime
    Time.now - @start_time
  end

  def echo(message)
    message
  end

  def reverse(str)
    str.reverse
  end

  def array_sum(arr)
    arr.sum
  end

  def hash_keys(hash)
    hash.keys
  end

  def test_data
    {
      string: "Hello from CRuby",
      number: 42,
      float: 3.14159,
      array: [1, 2, 3, 4, 5],
      hash: { a: 1, b: 2, c: 3 },
      symbol: :cruby,
      nil: nil,
      true: true,
      false: false
    }
  end
end

# Start DRb service on WebSocket
uri = 'ws://127.0.0.1:8080'
service = TestService.new

puts "Starting CRuby DRb WebSocket server..."
puts "URI: #{uri}"
puts "Service: TestService"
puts ""
puts "Available methods:"
puts "  - greet(name)       : Greet by name"
puts "  - add(a, b)         : Add two numbers"
puts "  - multiply(a, b)    : Multiply two numbers"
puts "  - increment         : Increment counter"
puts "  - counter           : Get current counter"
puts "  - uptime            : Server uptime in seconds"
puts "  - echo(message)     : Echo back message"
puts "  - reverse(str)      : Reverse a string"
puts "  - array_sum(arr)    : Sum array elements"
puts "  - hash_keys(hash)   : Get hash keys"
puts "  - test_data         : Get test data structure"
puts ""
puts "Press Ctrl+C to stop"
puts ""

DRb.start_service(uri, service)
DRb.thread.join
