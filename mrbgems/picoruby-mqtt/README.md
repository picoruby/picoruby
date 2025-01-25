# picoruby-mqtt
A MQTT client implementation for PicoRuby.

## Features
Current features:

- MQTT 3.1.1 protocol support
- Basic publish/subscribe functionality
- Automatic connection keep-alive with PING
- Built-in LED status indication for message reception (default callback)
- Customizable callback function

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

# Disconnect when done
client.disconnect
```

Example of overriding callback.


```ruby
require 'mqtt'

# By default, the onboard LED toggles when a message is received
# You can customize message handling by overriding the callback method
class DemoMQTTClient < MQTTClient
  def callback
    puts "Message received!"
  end
end

client = DemoMQTTClient.new("YOUR_IP_ADDRESS", 1883, "picoruby_test")

client.connect

client.publish("test/topic", "Hello from PicoRuby!")

client.subscribe("test/topic")

client.disconnect
```

## Roadmap

### Implemented
- Basic MQTT 3.1.1 protocol support
- Connection management
- Publish/Subscribe operations
- Keep-alive mechanism
- Ruby callback

### Planned
- TLS encryption support
- POSIX support
- MQTT 5.0 features
- QoS levels 1 and 2 support
- MQTT over WebSocket
- Last Will and Testament
- Over-the-Air (OTA) updates (callback script only)

### Not Planned
- MQTT-SN protocol
- Custom Certificate Authority (CA) chain support
