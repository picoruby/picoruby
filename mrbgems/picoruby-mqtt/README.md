# picoruby-mqtt

MQTT client for PicoRuby.

This is a pure Ruby implementation of MQTT 3.1.1 client for PicoRuby, designed for IoT devices.

## Features

- MQTT 3.1.1 protocol support
- QoS 0 (At most once delivery)
- CONNECT, PUBLISH, SUBSCRIBE, UNSUBSCRIBE, PING, DISCONNECT
- Keep-alive with automatic PING
- Clean session support
- Retained messages
- Uses TCPSocket from picoruby-socket-class

## Installation

Add to your `build_config.rb`:

```ruby
conf.gem :core => 'picoruby-mqtt'
```

## Usage

### Connect and Publish

```ruby
require 'mqtt'

# Connect to MQTT broker
client = MQTT::Client.new("test.mosquitto.org", 1883)
client.connect(client_id: "picoruby-device")

# Publish a message
client.publish("sensors/temperature", "25.5")

# Disconnect
client.disconnect
```

### Subscribe to Topics

```ruby
require 'mqtt'

client = MQTT::Client.new("test.mosquitto.org", 1883)
client.connect(client_id: "picoruby-subscriber")

# Subscribe to a topic
client.subscribe("sensors/#")

# Receive messages
5.times do
  topic, message = client.receive
  puts "#{topic}: #{message}"
end

client.disconnect
```

### With Block

```ruby
require 'mqtt'

MQTT::Client.connect("test.mosquitto.org", 1883, client_id: "picoruby") do |client|
  client.publish("hello/world", "Hello from PicoRuby!")
end
```

### Keep-Alive

```ruby
client = MQTT::Client.new("test.mosquitto.org", 1883)
client.connect(
  client_id: "picoruby-device",
  keep_alive: 60  # Send PING every 60 seconds
)

# Client will automatically send PING requests
# to keep the connection alive
```

### Retained Messages

```ruby
client = MQTT::Client.new("test.mosquitto.org", 1883)
client.connect(client_id: "picoruby")

# Publish a retained message
client.publish("device/status", "online", retain: true)

client.disconnect
```

## API Reference

### MQTT::Client

#### Class Methods

- `MQTT::Client.new(host, port = 1883)` - Create new MQTT client
- `MQTT::Client.connect(host, port = 1883, **options) { |client| ... }` - Connect with block

#### Instance Methods

- `connect(**options)` - Connect to broker
  - `client_id:` - Client identifier (default: random)
  - `keep_alive:` - Keep-alive interval in seconds (default: 60)
  - `clean_session:` - Clean session flag (default: true)
  - `username:` - Username for authentication
  - `password:` - Password for authentication

- `publish(topic, payload, retain: false, qos: 0)` - Publish message
  - `topic` - Topic name
  - `payload` - Message payload (string)
  - `retain` - Retain flag (default: false)
  - `qos` - Quality of Service (0 only, default: 0)

- `subscribe(*topics, qos: 0)` - Subscribe to topics
  - `topics` - One or more topic filters
  - `qos` - Quality of Service (0 only, default: 0)

- `unsubscribe(*topics)` - Unsubscribe from topics

- `receive(timeout: nil)` - Receive next message
  - Returns `[topic, payload]` or `nil` on timeout

- `ping` - Send PING request

- `disconnect` - Disconnect from broker

- `connected?` - Check connection status

## Supported Features

- ✅ CONNECT / CONNACK
- ✅ PUBLISH (QoS 0)
- ✅ SUBSCRIBE / SUBACK
- ✅ UNSUBSCRIBE / UNSUBACK
- ✅ PINGREQ / PINGRESP
- ✅ DISCONNECT
- ✅ Keep-alive
- ✅ Retained messages
- ✅ Clean session
- ✅ Username/password authentication
- ⚠️ QoS 1/2 not supported
- ⚠️ Will message not supported

## Example: IoT Sensor

```ruby
require 'mqtt'

def read_temperature
  # Read from sensor
  22.5 + rand * 3
end

client = MQTT::Client.new("mqtt.example.com", 1883)
client.connect(
  client_id: "sensor-001",
  keep_alive: 30
)

# Publish sensor data every 5 seconds
loop do
  temp = read_temperature
  client.publish("sensors/temperature", temp.to_s)
  sleep 5
end
```

## License

MIT
