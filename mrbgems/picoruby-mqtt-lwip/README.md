# picoruby-mqtt-lwip

lwIP native MQTT client for PicoRuby (RP2040 optimized).

This is a native lwIP implementation of MQTT 3.1.1 client for PicoRuby, designed for high-performance IoT applications on RP2040-based boards.

## Features

- Native lwIP MQTT implementation for better performance
- MQTT 3.1.1 protocol support
- QoS 0 (At most once delivery)
- CONNECT, PUBLISH, SUBSCRIBE, UNSUBSCRIBE, PING, DISCONNECT
- Keep-alive with automatic PING
- Clean session support
- API compatible with picoruby-net-mqtt
- Optimized for RP2040 (pico_w) boards

## Installation

Add to your `build_config.rb`:

```ruby
conf.gem core: 'picoruby-mqtt-lwip'
```

## Usage

The API is identical to picoruby-net-mqtt:

### Connect and Publish

```ruby
require 'net/mqtt'

# Connect to MQTT broker
client = Net::MQTT::Client.new("test.mosquitto.org", 1883)
client.connect

# Publish a message
client.publish("sensors/temperature", "25.5")

# Disconnect
client.disconnect
```

### Subscribe to Topics

```ruby
require 'net/mqtt'

client = Net::MQTT::Client.new("test.mosquitto.org", 1883)
client.connect

# Subscribe to a topic
client.subscribe("sensors/#")

# Receive messages
5.times do
  topic, message = client.get
  puts "#{topic}: #{message}"
end

client.disconnect
```

### With Block

```ruby
require 'net/mqtt'

Net::MQTT::Client.connect("test.mosquitto.org", 1883) do |client|
  client.publish("hello/world", "Hello from PicoRuby!")
end
```

## When to Use

### Use picoruby-mqtt-lwip when:
- Running on RP2040-based boards (pico_w)
- Performance is critical
- Using mruby/c VM
- Need low memory footprint

### Use picoruby-net-mqtt instead when:
- Running on other platforms
- Pure Ruby implementation is preferred
- Development/debugging (easier to modify)
- Maximum compatibility is needed

## Requirements

- RP2040 board with WiFi (pico_w)
- picoruby-socket with lwIP MQTT support enabled
- mruby/c or mruby VM

## Performance

Compared to picoruby-net-mqtt (pure Ruby):
- **Memory**: Lower memory usage due to native implementation
- **Speed**: Faster message processing via lwIP
- **CPU**: Reduced CPU overhead for protocol handling

## API Compatibility

This gem provides the same `Net::MQTT` module and API as picoruby-net-mqtt, making them interchangeable. Simply change the gem specification in your build config to switch implementations.

## Example: IoT Sensor

```ruby
require 'net/mqtt'

def read_temperature
  # Read from sensor
  22.5 + rand * 3
end

client = Net::MQTT::Client.new("mqtt.example.com", 1883)
client.connect

# Publish sensor data every 5 seconds
loop do
  temp = read_temperature
  client.publish("sensors/temperature", temp.to_s)
  sleep 5
end
```

## License

MIT