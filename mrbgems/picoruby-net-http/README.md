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

# Advanced usage with instance
http = Net::HTTP.new("example.com", 443)
http.use_ssl = true  # Required for HTTPS
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

- `url`: Hostname with protocol (`https://example.com`) or plain hostname (`example.com` for HTTP only)
- `path`: Request path (e.g., `/get`, `/api/data`)
- `port`: Port number (443 for HTTPS, 80 for HTTP)

**Important:** For HTTPS connections, you must include the `https://` prefix in the URL. Without it, the library defaults to plain HTTP even if port 443 is specified.

```ruby
# Correct HTTPS usage
Net::HTTP.get("https://example.com", "/", 443)

# Wrong - will try HTTP on port 443 and fail
Net::HTTP.get("example.com", "/", 443)
```

### Net::HTTP.new(host, port)

Creates a new HTTP client instance.

```ruby
# HTTP
http = Net::HTTP.new("example.com", 80)
response = http.get("/path")

# HTTPS - must set use_ssl = true
http = Net::HTTP.new("example.com", 443)
http.use_ssl = true
response = http.get("/path")
response = http.post("/path", "data")
```

## License

MIT
