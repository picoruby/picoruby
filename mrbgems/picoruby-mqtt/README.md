# picoruby-mqtt
A MQTT client implementation for PicoRuby with API compatibility with ruby-mqtt.

## Features
Current features:

- MQTT 3.1.1 protocol support
- Basic publish/subscribe functionality
- Automatic connection keep-alive with PING
- Built-in LED status indication for message reception (default callback)
- Customizable callback function
- QoS 1 support for publish operations
- POSIX platform compatibility (via LwIP and picoruby-net)
- ruby-mqtt compatible API

## Usage

Must be connected to WiFi in advance.

### Ruby-MQTT Style API

```ruby
require 'mqtt'

# Using block style (automatically disconnects)
MQTT::Client.connect('test.mosquitto.org') do |client|
  # Publish a message
  client.publish('test/topic', 'Hello from PicoRuby!')

  # Subscribe and receive messages
  client.get('test/topic') do |topic, message|
    puts "#{topic}: #{message}"
  end
end

# Or without block
client = MQTT::Client.connect('test.mosquitto.org')
client.publish('test/topic', 'Hello!')
client.disconnect

# Hash style initialization
client = MQTT::Client.new(
  host: 'test.mosquitto.org',
  port: 1883,
  client_id: 'picoruby_test'
)
client.connect
```

## Roadmap

### Implemented
- Basic MQTT 3.1.1 protocol support
- Connection management
- Publish/Subscribe operations
- Keep-alive mechanism
- Ruby callback system
- QoS 1 for publish operations
- POSIX platform compatibility (via LwIP abstraction)
- ruby-mqtt compatible API

### In Progress
- TLS encryption support (dependencies added, implementation pending)
- Complete QoS 1 and 2 support (partial QoS 1 implemented)

### Planned
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
- picoruby-cyw43: For WiFi connectivity and LED control on Pico W

