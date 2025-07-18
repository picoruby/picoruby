# picoruby-mqtt
A MQTT client implementation for PicoRuby with API compatibility with ruby-mqtt.

## Features
Current features:

- MQTT 3.1.1 protocol support
- Basic publish/subscribe functionality (QoS 0)
- Automatic connection management
- POSIX platform compatibility (via LwIP and picoruby-net)
- ruby-mqtt compatible API

## Usage

Must be connected to WiFi in advance.

### Ruby-MQTT Style API

```ruby
require 'mqtt'

# Initialize client
client = MQTT::Client.new(host: 'test.mosquitto.org', port: 1883)
client.connect('my_client_id')

# Publish a message
client.publish('test/topic', 'Hello from PicoRuby!')

# Subscribe to a topic
client.subscribe('test/topic')

# Get messages (polling style)
loop do
  topic, payload = client.get
  if topic && payload
    puts "#{topic}: #{payload}"
  end
  sleep 0.1
end

client.disconnect
```

Or you can use Non-Blocking style.

```ruby
client.get do |topic, payload|
  puts "#{topic}: #{payload}"
end
```

## Roadmap

### Implemented
- Basic MQTT 3.1.1 protocol support
- Connection management
- Publish/Subscribe operations (QoS 0)
- POSIX platform compatibility (via LwIP abstraction)
- ruby-mqtt compatible API

### In Progress
- QoS 1 support for publish operations
- TLS encryption support (dependencies added, implementation pending)

### Planned
- Complete QoS 1 and 2 support
- MQTT 5.0 features
- MQTT over WebSocket
- Last Will and Testament
- Over-the-Air (OTA) updates (callback script only)

### Not Planned
- MQTT-SN protocol
- Custom Certificate Authority (CA) chain support

## Platform Support

- RP2040 (Raspberry Pi Pico): Fully supported and tested
- POSIX: Basic support via LwIP abstraction layer

## Dependencies

- picoruby-net: Network abstraction layer
- picoruby-mbedtls: TLS support (for future encrypted connections)
- picoruby-cyw43: For WiFi connectivity on Pico W

