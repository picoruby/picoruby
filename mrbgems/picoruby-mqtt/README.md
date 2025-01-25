# picoruby-mqtt
A lightweight MQTT client implementation for PicoRuby, designed specifically for Raspberry Pi Pico W.

## Features
Current features:

- MQTT 3.1.1 protocol support
- Basic publish/subscribe functionality
- Automatic connection keep-alive with PING
- Ruby-based callback system
- Built-in LED status indication for message reception
- Customizable message handling

## Usage

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

# By default, the onboard LED toggles when a message is received
# You can customize message handling by overriding the callback method
class DemoMQTTClient < MQTTClient
  def callback
    # Your custom message handling logic here
    puts "Message received!"
  end
end

# Disconnect when done
client.disconnect
```

## Roadmap

### Implemented
- Basic MQTT 3.1.1 protocol support
- Connection management
- Publish/Subscribe operations
- Keep-alive mechanism
- Ruby callback
- LED status indication

### Planned
- TLS encryption support
- POSIX support
- MQTT 5.0 features
- QoS levels 1 and 2 support
- MQTT over WebSocket
- Last Will and Testament
- Over-the-Air (OTA) updates (Callback script only)

### Not Planned
- MQTT-SN protocol
- Custom Certificate Authority (CA) chain support
