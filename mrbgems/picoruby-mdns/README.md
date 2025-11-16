# picoruby-mdns

mDNS (Multicast DNS) client and responder for PicoRuby.

This is a pure Ruby implementation of mDNS (RFC 6762) for PicoRuby, providing service discovery and hostname resolution on local networks.

## Features

- mDNS Client - Query for services and resolve hostnames
- mDNS Responder - Advertise services and respond to queries
- Pure Ruby implementation using UDPSocket

## Usage

### Client - Query for services

```ruby
require 'mdns'

# Create a client
client = MDNS::Client.new

# Query for a specific service type
services = client.query("_http._tcp.local")

# Query for a hostname
address = client.resolve("mydevice.local")
```

### Responder - Advertise a service

```ruby
require 'mdns'

# Create a responder
responder = MDNS::Responder.new("mydevice.local", "192.168.1.100")

# Advertise a service
responder.advertise(
  service_name: "My Web Server",
  service_type: "_http._tcp.local",
  port: 80,
  txt_records: {"path" => "/"}
)

# Start responding to queries
responder.run
```

## Supported Record Types

- A (IPv4 address)
- PTR (Pointer for service discovery)
- SRV (Service locator)
- TXT (Text records)

## License

MIT
