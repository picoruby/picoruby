#!/usr/bin/env ruby
# frozen_string_literal: true

# Unified DRb interop test script
# Works with both CRuby and PicoRuby
#
# Test A: CRuby server + PicoRuby client
#   Terminal 1: ruby interop.rb server
#   Terminal 2: picoruby interop.rb client
#
# Test B: PicoRuby server + CRuby client
#   Terminal 1: picoruby interop.rb server
#   Terminal 2: ruby interop.rb client

if RUBY_ENGINE == 'ruby'
  require 'bundler/inline'
  gemfile do
    source 'https://rubygems.org'
    gem 'drb'
  end
elsif RUBY_ENGINE == 'mruby'
  require 'drb'
else
  raise "Unsupported Ruby engine: #{RUBY_ENGINE}"
end

SERVER_URI = "druby://localhost:8787"

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
    raise "test error"
  end
end

def run_server
  front = TestService.new
  DRb.start_service(SERVER_URI, front)
  puts "DRb server started on #{SERVER_URI}"
  puts "Press Ctrl+C to stop"
  DRb.thread.join
end

def run_client
  DRb.start_service
  obj = DRb::DRbObject.new_with_uri(SERVER_URI)

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
  rescue DRb::DRbUnknownError => e
    puts "PASS: raise_error raised DRbUnknownError: #{e.message}"
    passed += 1
  rescue RuntimeError => e
    puts "PASS: raise_error raised RuntimeError: #{e.message}"
    passed += 1
  rescue => e
    puts "PASS: raise_error raised #{e.class}: #{e.message}"
    passed += 1
  end

  puts ""
  puts "Results: #{passed} passed, #{failed} failed"
  exit(failed > 0 ? 1 : 0)
end

case ARGV[0]
when "server"
  run_server
when "client"
  run_client
else
  puts "Usage: #{File.basename($0)} [server|client]"
  puts "  server - Start DRb server on #{SERVER_URI}"
  puts "  client - Connect to DRb server on #{SERVER_URI}"
  exit 1
end
