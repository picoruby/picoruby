# picoruby-net-http

A lightweight HTTP client library for PicoRuby.

## Features

- HTTP and HTTPS support
- GET, POST, PUT, DELETE, and other HTTP methods
- Custom headers
- SSL/TLS support via picoruby-socket

## Basic Usage

```ruby
require 'net/http'

# Simple GET request
response = Net::HTTP.get("https://example.com", "/path", 443)
puts response

# Using URI
uri = URI.parse("https://api.example.com/data")
response = Net::HTTP.get(uri)

# Advanced usage
http = Net::HTTP.new("example.com", 443)
response = http.get("/path")
puts response.body
```

## Testing on Microcontrollers

When testing HTTPS on microcontrollers (RP2040, RP2350, etc.), it's recommended to use `httpbin.org` as it's lightweight and less likely to cause timeouts:

```ruby
require 'net/http'

# Recommended for microcontroller testing
response = Net::HTTP.get("https://httpbin.org", "/get", 443)
puts response
```

Note: Other sites may be too heavy and cause timeouts on resource-constrained devices.

## API

### Net::HTTP.get(url, path, port)

Performs a GET request and returns the response body as a string.

- `url`: Hostname (with or without `https://` prefix)
- `path`: Request path (e.g., `/get`, `/api/data`)
- `port`: Port number (443 for HTTPS, 80 for HTTP)

### Net::HTTP.new(host, port)

Creates a new HTTP client instance.

```ruby
http = Net::HTTP.new("example.com", 443)
response = http.get("/path")
response = http.post("/path", "data")
```

## License

MIT
