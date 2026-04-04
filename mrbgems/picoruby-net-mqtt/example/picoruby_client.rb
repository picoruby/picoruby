# PicoRuby MQTT client test
#
# Usage:
#   Terminal 1: ruby cruby_server.rb
#   Terminal 2: picoruby picoruby_client.rb

require 'net/mqtt'

HOST = "localhost"
PORT = 1883

passed = 0
failed = 0

puts "Connecting to MQTT broker at #{HOST}:#{PORT}..."

begin
  client = Net::MQTT::Client.new(HOST, PORT,
    client_id: "picoruby-test",
    keep_alive: 60,
    clean_session: true
  )
  client.connect
  puts "PASS: Connected"
  passed += 1
rescue => e
  puts "FAIL: Connection error: #{e.class}: #{e.message}"
  exit 1
end

# Subscribe before publishing so we receive our own messages back
begin
  client.subscribe("picoruby/test/#")
  puts "PASS: Subscribed to picoruby/test/#"
  passed += 1
rescue => e
  puts "FAIL: Subscribe error: #{e.message}"
  failed += 1
end

# Test 1: basic publish/receive
begin
  client.publish("picoruby/test/hello", "Hello from PicoRuby!")
  topic, payload = client.receive(timeout: 5)
  if topic == "picoruby/test/hello" && payload == "Hello from PicoRuby!"
    puts "PASS: Publish/receive - #{topic}: #{payload.inspect}"
    passed += 1
  else
    puts "FAIL: Publish/receive - got topic=#{topic.inspect} payload=#{payload.inspect}"
    failed += 1
  end
rescue => e
  puts "FAIL: Publish/receive raised #{e.class}: #{e.message}"
  failed += 1
end

# Test 2: wildcard topic match
begin
  client.publish("picoruby/test/sensor/temp", "25.3")
  topic, payload = client.receive(timeout: 5)
  if topic == "picoruby/test/sensor/temp" && payload == "25.3"
    puts "PASS: Wildcard topic - #{topic}: #{payload.inspect}"
    passed += 1
  else
    puts "FAIL: Wildcard topic - got topic=#{topic.inspect}"
    failed += 1
  end
rescue => e
  puts "FAIL: Wildcard topic raised #{e.class}: #{e.message}"
  failed += 1
end

# Test 3: ping
begin
  client.ping
  puts "PASS: Ping/Pong"
  passed += 1
rescue => e
  puts "FAIL: Ping raised #{e.class}: #{e.message}"
  failed += 1
end

# Test 4: multiple messages
begin
  messages = ["First", "Second", "Third"]
  mi = 0
  while mi < messages.size
    client.publish("picoruby/test/multi", messages[mi])
    mi += 1
  end
  received = []
  ri = 0
  while ri < messages.size
    topic, payload = client.receive(timeout: 5)
    received << payload if topic == "picoruby/test/multi"
    ri += 1
  end
  if received == messages
    puts "PASS: Multiple messages"
    passed += 1
  else
    puts "FAIL: Multiple messages - got #{received.inspect}"
    failed += 1
  end
rescue => e
  puts "FAIL: Multiple messages raised #{e.class}: #{e.message}"
  failed += 1
end

# Test 5: disconnect
begin
  client.disconnect
  unless client.connected?
    puts "PASS: Disconnected gracefully"
    passed += 1
  else
    puts "FAIL: Still connected after disconnect"
    failed += 1
  end
rescue => e
  puts "FAIL: Disconnect raised #{e.class}: #{e.message}"
  failed += 1
end

puts ""
puts "=" * 40
puts "Results: #{passed} passed, #{failed} failed"
puts "=" * 40
