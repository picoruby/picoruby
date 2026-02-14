#!/usr/bin/env ruby
# frozen_string_literal: true

# PicoRuby interop test script
# Run with: picoruby picoruby_interop.rb [client|server]
#
# Test A: CRuby server + PicoRuby client
#   Terminal 1: ruby cruby_interop.rb server
#   Terminal 2: picoruby picoruby_interop.rb client
#
# Test B: PicoRuby server + CRuby client
#   Terminal 1: picoruby picoruby_interop.rb server
#   Terminal 2: ruby cruby_interop.rb client

require 'drb'

SERVER_URI = "druby://localhost:8788"
CLIENT_TARGET_URI = "druby://localhost:8788"

class TestService
  def hello(name)
    "Hello, #{name}!"
  end

  def add(a, b)
    a + b
  end

  def echo(arg)
    arg
  end

  def raise_error
    raise "test error from PicoRuby"
  end
end

def run_server
  front = TestService.new
  server = DRb::DRbServer.new(SERVER_URI, front)
  server.start
  puts "PicoRuby DRb server started on #{SERVER_URI}"
  puts "Press Ctrl+C to stop"
  server.run
end

def run_client
  obj = DRb::DRbObject.new(CLIENT_TARGET_URI)

  passed = 0
  failed = 0

  # Test: string argument and return
  begin
    result = obj.hello("world")
    if result == "Hello, world!"
      puts "PASS: hello('world') => #{result.inspect}"
      passed += 1
    else
      puts "FAIL: hello('world') => #{result.inspect} (expected 'Hello, world!')"
      failed += 1
    end
  rescue => e
    puts "FAIL: hello('world') raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test: numeric arguments and return
  begin
    result = obj.add(2, 3)
    if result == 5
      puts "PASS: add(2, 3) => #{result.inspect}"
      passed += 1
    else
      puts "FAIL: add(2, 3) => #{result.inspect} (expected 5)"
      failed += 1
    end
  rescue => e
    puts "FAIL: add(2, 3) raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test: array echo
  begin
    result = obj.echo([1, "two", 3])
    if result == [1, "two", 3]
      puts "PASS: echo([1, 'two', 3]) => #{result.inspect}"
      passed += 1
    else
      puts "FAIL: echo([1, 'two', 3]) => #{result.inspect}"
      failed += 1
    end
  rescue => e
    puts "FAIL: echo([1, 'two', 3]) raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test: hash echo
  begin
    result = obj.echo({a: 1})
    if result == {a: 1}
      puts "PASS: echo({a: 1}) => #{result.inspect}"
      passed += 1
    else
      puts "FAIL: echo({a: 1}) => #{result.inspect}"
      failed += 1
    end
  rescue => e
    puts "FAIL: echo({a: 1}) raised #{e.class}: #{e.message}"
    failed += 1
  end

  # Test: exception propagation
  begin
    obj.raise_error
    puts "FAIL: raise_error did not raise"
    failed += 1
  rescue => e
    puts "PASS: raise_error raised #{e.class}: #{e.message}"
    passed += 1
  end

  puts ""
  puts "Results: #{passed} passed, #{failed} failed"
end

case ARGV[0]
when "server"
  run_server
when "client"
  run_client
else
  puts "Usage: picoruby picoruby_interop.rb [server|client]"
  puts "  server - Start PicoRuby DRb server on #{SERVER_URI}"
  puts "  client - Connect to CRuby DRb server on #{CLIENT_TARGET_URI}"
end
