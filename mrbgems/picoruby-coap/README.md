# picoruby-coap

CoAP (Constrained Application Protocol) client and server for PicoRuby.

This is a pure Ruby implementation of CoAP (RFC 7252) for PicoRuby, designed for IoT devices and constrained networks.

## Features

- CoAP Client - Send GET, POST, PUT, DELETE requests
- CoAP Server - Handle requests and send responses
- Confirmable (CON) and Non-confirmable (NON) messages
- Token-based request/response matching
- Common CoAP options (Uri-Path, Content-Format, etc.)
- Pure Ruby implementation using UDPSocket

## Usage

### Client - Send requests

```ruby
require 'coap'

# Create a client
client = CoAP::Client.new

# Send a GET request
response = client.get("coap://192.168.1.100:5683/temperature")
puts response.payload

# Send a POST request
response = client.post("coap://192.168.1.100:5683/led", "on")
```

### Server - Handle requests

```ruby
require 'coap'

# Create a server
server = CoAP::Server.new(port: 5683)

# Register a resource handler
server.add_resource("/temperature") do |request|
  CoAP::Response.new(
    code: CoAP::CODE_CONTENT,
    payload: "25.5"
  )
end

# Start the server
server.run
```

## Supported Methods

- GET (0.01)
- POST (0.02)
- PUT (0.03)
- DELETE (0.04)

## Supported Response Codes

- 2.01 Created
- 2.02 Deleted
- 2.03 Valid
- 2.04 Changed
- 2.05 Content
- 4.00 Bad Request
- 4.04 Not Found
- 5.00 Internal Server Error

## License

MIT
