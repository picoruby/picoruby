# picoruby-base64

Base64 encoding/decoding library for PicoRuby.

## Usage

```ruby
require 'base64'

# Standard Base64 encoding
encoded = Base64.encode64("Hello, World!")
puts encoded

# Standard Base64 decoding
decoded = Base64.decode64(encoded)
puts decoded  # => "Hello, World!"

# URL-safe Base64 encoding
url_encoded = Base64.urlsafe_encode64("data")
url_decoded = Base64.urlsafe_decode64(url_encoded)
```

## API

### Methods

- `Base64.encode64(string)` - Encode string to standard Base64
- `Base64.decode64(string)` - Decode standard Base64 string
- `Base64.urlsafe_encode64(string)` - Encode to URL-safe Base64
- `Base64.urlsafe_decode64(string)` - Decode URL-safe Base64 string

## Notes

- Standard Base64 uses `+`, `/` and `=` characters
- URL-safe Base64 uses `-`, `_` instead of `+`, `/`
