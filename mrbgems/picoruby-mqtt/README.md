# picoruby-mqtt
A MQTT client implementation for PicoRuby.

## Features
Current features:

- MQTT 3.1.1 protocol support
- Basic publish/subscribe functionality
- Automatic connection keep-alive with PING
- Built-in LED status indication for message reception (default callback)
- Customizable callback function
- QoS 1 support for publish operations
- POSIX platform compatibility (via LwIP and picoruby-net)

## Usage

Must be connected to WiFi in advance.
Basic example.

```ruby
require 'mqtt'

# Initialize client
client = MQTTClient.new("YOUR_IP_ADDRESS", 1883, "picoruby_test")

# Connect to broker
client.connect

# Publish message
client.publish("test/topic", "Hello from PicoRuby!")

# Subscribe to topic
client.subscribe("test/topic")

# Wait for messages in a loop
client.wait_for_messages

# Disconnect when done
client.disconnect
```

Example of overriding callback.

```ruby
require 'mqtt'

# By default, the onboard LED toggles when a message is received
# You can customize message handling by overriding the callback method
client = MQTTClient.new("YOUR_IP_ADDRESS", 1883, "picoruby_test")

# Set custom callback
client.on_message do |topic, payload|
  puts "Received on #{topic}: #{payload}"
  # Your custom handling here
end

client.connect
client.subscribe("test/topic")
client.wait_for_messages
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
