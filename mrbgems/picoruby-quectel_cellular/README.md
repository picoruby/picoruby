# picoruby-quectel_cellular

Quectel cellular module driver for PicoRuby.

## Overview

This gem provides a Ruby interface for controlling Quectel cellular modem modules.
It supports UDP and HTTPS communication through AT commands over UART.

## Supported Modules

- EC21 - LTE Cat 1
- EC20 - LTE Cat 4
- BG96 - LTE Cat M1/NB1

## Features

- Basic module control and SIM card management
- UDP client communication
- HTTPS client with TLS support
- Soracom Beam UDP integration
- File upload/download to module storage
- Command logging for debugging

## Usage

### UDP Communication

```ruby
require 'quectel_cellular'

uart = UART.new(unit: :RP2040_UART1, txd_pin: 8, rxd_pin: 9, baudrate: 115200)
client = QuectelCellular::UDPClient.new(uart: uart)
client.check_sim_status
client.configure_and_activate_context
client.send("example.com", 1234, "Hello, World!")
```

### HTTPS Communication

See [example/ambient.rb](example/ambient.rb) for a complete example of sending sensor data to Ambient IoT service via HTTPS.

### Module Reset (if PERST pin is connected)

```ruby
client.perst = GPIO.new(21, GPIO::OUT)
client.reset
```

## Configuration

Default APN settings are configured for Soracom:

```ruby
client.apn = "SORACOM.IO"
client.username = "sora"
client.password = "sora"
client.user_agent = "picoruby"
```

## API Reference

See [sig/quectel_cellular.rbs](sig/quectel_cellular.rbs) for complete API signatures.
