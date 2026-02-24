#!/usr/bin/env picoruby

# PicoRuby DRb WebSocket interop test script
# Run with: picoruby websocket.rb [server|client]
#
# Usage:
#   Server mode: picoruby websocket.rb server
#   Client mode: picoruby websocket.rb client [ws://host:port]

require 'drb'

SERVER_URI = 'ws://0.0.0.0:9090'
DEFAULT_CLIENT_URI = 'ws://localhost:9090'

# Sample service object
class SensorService
  def initialize
    @counter = 0
  end

  def hello(name)
    puts "[#{Time.now}] hello(#{name.inspect})"
    "Hello from PicoRuby, #{name}!"
  end

  def temperature
    puts "[#{Time.now}] temperature()"
    # Simulate sensor reading
    20 + (RNG.random_int % 10)
  end

  def increment
    puts "[#{Time.now}] increment()"
    @counter += 1
  end

  def counter
    puts "[#{Time.now}] counter()"
    @counter
  end

  def echo(message)
    puts "[#{Time.now}] echo(#{message.inspect})"
    message
  end

  def current_time
    puts "[#{Time.now}] current_time()"
    Time.now.to_s
  end

  def data_types_test
    puts "[#{Time.now}] data_types_test()"
    {
      string: "Hello",
      integer: 42,
      float: 3.14,
      array: [1, 2, 3],
      hash: { a: 1, b: 2 },
      symbol: :test,
      nil_value: nil,
      true_value: true,
      false_value: false
    }
  end
end

def run_server
  service = SensorService.new

  puts "Starting DRb WebSocket server..."
  puts "URI: #{SERVER_URI}"
  puts "Service: SensorService"
  puts ""
  puts "Available methods:"
  puts "  - hello(name)       : Greet by name"
  puts "  - temperature       : Get simulated temperature"
  puts "  - increment         : Increment counter"
  puts "  - counter           : Get current counter"
  puts "  - echo(message)     : Echo back message"
  puts "  - current_time      : Get current time"
  puts "  - data_types_test   : Test various data types"
  puts ""
  puts "Press Ctrl+C to stop"
  puts ""

  DRb.start_service(SERVER_URI, service)
  DRb.thread.join
end

def run_client(uri)
  puts "Connecting to DRb WebSocket server..."
  puts "URI: #{uri}"
  puts ""

  # Create reference to remote object
  service = DRb::DRbObject.new_with_uri(uri)

  passed = 0
  failed = 0

  # Test 1: Simple method call
  begin
    puts "Test 1: Hello"
    result = service.hello('PicoRuby Client')
    if result.include?("Hello from PicoRuby")
      puts "  PASS: #{result}"
      passed += 1
    else
      puts "  FAIL: unexpected result: #{result}"
      failed += 1
    end
    puts ""
  rescue => e
    puts "  FAIL: #{e.class}: #{e.message}"
    failed += 1
    puts ""
  end

  # Test 2: Method with no arguments
  begin
    puts "Test 2: Temperature"
    temp = service.temperature
    if temp.is_a?(Integer) && temp >= 20 && temp < 30
      puts "  PASS: Temperature: #{temp}°C"
      passed += 1
    else
      puts "  FAIL: unexpected temperature: #{temp}"
      failed += 1
    end
    puts ""
  rescue => e
    puts "  FAIL: #{e.class}: #{e.message}"
    failed += 1
    puts ""
  end

  # Test 3: State manipulation
  begin
    puts "Test 3: Counter"
    initial = service.counter
    service.increment
    after_one = service.counter
    service.increment
    after_two = service.counter

    if after_two > after_one && after_one > initial
      puts "  PASS: Counter: #{initial} -> #{after_one} -> #{after_two}"
      passed += 1
    else
      puts "  FAIL: Counter behavior incorrect"
      failed += 1
    end
    puts ""
  rescue => e
    puts "  FAIL: #{e.class}: #{e.message}"
    failed += 1
    puts ""
  end

  # Test 4: Echo test
  begin
    puts "Test 4: Echo"
    message = "Hello from PicoRuby!"
    echo = service.echo(message)
    if message == echo
      puts "  PASS: Echo matched"
      passed += 1
    else
      puts "  FAIL: Echo mismatch: sent '#{message}', received '#{echo}'"
      failed += 1
    end
    puts ""
  rescue => e
    puts "  FAIL: #{e.class}: #{e.message}"
    failed += 1
    puts ""
  end

  # Test 5: Time
  begin
    puts "Test 5: Current Time"
    time = service.current_time
    if time.is_a?(String) && time.length > 0
      puts "  PASS: Server time: #{time}"
      passed += 1
    else
      puts "  FAIL: unexpected time format: #{time.inspect}"
      failed += 1
    end
    puts ""
  rescue => e
    puts "  FAIL: #{e.class}: #{e.message}"
    failed += 1
    puts ""
  end

  # Test 6: Data types
  begin
    puts "Test 6: Data Types"
    result = service.data_types_test
    puts "  Result class: #{result.class}"
    puts "  Result value: #{result.inspect}"
    if result.is_a?(Hash)
      puts "  Checking individual fields..."
      errors = []
      errors << "string: expected 'Hello', got #{result[:string].inspect}" unless result[:string] == "Hello"
      errors << "integer: expected 42, got #{result[:integer].inspect}" unless result[:integer] == 42
      errors << "float: expected 3.14, got #{result[:float].inspect}" unless result[:float] == 3.14
      errors << "array: expected [1, 2, 3], got #{result[:array].inspect}" unless result[:array] == [1, 2, 3]
      errors << "hash: expected {a: 1, b: 2}, got #{result[:hash].inspect}" unless result[:hash] == { a: 1, b: 2 }
      errors << "symbol: expected :test, got #{result[:symbol].inspect}" unless result[:symbol] == :test
      errors << "nil_value: expected nil, got #{result[:nil_value].inspect}" unless result[:nil_value] == nil
      errors << "true_value: expected true, got #{result[:true_value].inspect}" unless result[:true_value] == true
      errors << "false_value: expected false, got #{result[:false_value].inspect}" unless result[:false_value] == false

      if errors.empty?
        puts "  PASS: All data types correctly marshalled"
        passed += 1
      else
        puts "  FAIL: #{errors.length} field(s) failed:"
        errors.each { |e| puts "    - #{e}" }
        failed += 1
      end
    else
      puts "  FAIL: Expected Hash, got #{result.class}"
      failed += 1
    end
    puts ""
  rescue => e
    puts "  FAIL: #{e.class}: #{e.message}"
    failed += 1
    puts ""
  end

  puts "=========================================="
  puts "Results: #{passed} passed, #{failed} failed"
  puts "=========================================="
end

case ARGV[0]
when "server"
  run_server
when "client"
  uri = ARGV[1] || DEFAULT_CLIENT_URI
  run_client(uri)
else
  puts "Usage: microruby websocket.rb [server|client] [uri]"
  puts "  server           - Start PicoRuby DRb WebSocket server on #{SERVER_URI}"
  puts "  client [uri]     - Connect to DRb WebSocket server (default: #{DEFAULT_CLIENT_URI})"
  puts ""
  puts "Examples:"
  puts "  bin/microruby websocket.rb server"
  puts "  bin/microruby websocket.rb client"
  puts "  bin/microruby websocket.rb client ws://192.168.1.100:8080"
end
