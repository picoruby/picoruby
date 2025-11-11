# picoruby-net

Network communication library for PicoRuby (HTTP/HTTPS, TCP/UDP, DNS).

## Usage

### HTTP Client

```ruby
require 'net'

# HTTP request
client = Net::HTTPClient.new("example.com", 80)
response = client.get("/api/data")
puts response[:status]   # => 200
puts response[:body]     # Response body

# With custom headers
response = client.get_with_headers("/api", {"User-Agent" => "PicoRuby"})

# POST request
response = client.post(
  "/api/submit",
  {"Content-Type" => "application/json"},
  '{"key":"value"}'
)
```

### HTTPS Client

```ruby
# HTTPS request (uses TLS)
client = Net::HTTPSClient.new("api.example.com", 443)
response = client.get("/secure/data")
```

### TCP Client

```ruby
# Send TCP request
response = Net::TCPClient.request("example.com", 80, "GET / HTTP/1.0\r\n\r\n", false)

# With TLS
response = Net::TCPClient.request("example.com", 443, "GET / HTTP/1.0\r\n\r\n", true)
```

### UDP Client

```ruby
# Send UDP datagram
response = Net::UDPClient.send("example.com", 1234, "Hello", false)

# With DTLS
response = Net::UDPClient.send("example.com", 5684, "Hello", true)
```

### DNS

```ruby
# Resolve hostname
ip = Net::DNS.resolve("example.com", true)  # true for TCP
puts ip  # => "93.184.216.34"
```

## API

### Net::HTTPClient / Net::HTTPSClient

- `new(host, port)` - Create HTTP/HTTPS client
- `get(path)` - GET request
- `get_with_headers(path, headers)` - GET with custom headers
- `post(path, headers, body)` - POST request
- `put(path, headers, body)` - PUT request

### Response Format

All HTTP methods return a hash:
```ruby
{
  status: 200,              # HTTP status code
  headers: {...},           # Response headers
  body: "response content"  # Response body
}
```

## Notes

- HTTPS/TLS requires mbedtls library
- Default ports: HTTP (80), HTTPS (443)
