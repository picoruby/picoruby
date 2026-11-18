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

The methods work as the methods of CRuby's base64 library do.

- `Base64.encode64(string)` - Encode to the RFC 2045 form. A newline follows every 60 characters and the last line.
- `Base64.strict_encode64(string)` - Encode to the RFC 4648 form, with no newline.
- `Base64.decode64(string)` - Decode leniently. The decoder skips characters outside the alphabet and stops at the padding.
- `Base64.strict_decode64(string)` - Decode the RFC 4648 form. Other input raises `ArgumentError`.
- `Base64.urlsafe_encode64(string, padding: true)` - Encode to the URL-safe form. Give `padding: false` to omit `=`.
- `Base64.urlsafe_decode64(string)` - Decode the URL-safe form, with or without padding. Other input raises `ArgumentError`.

## Notes

- Standard Base64 uses `+`, `/` and `=` characters.
- URL-safe Base64 uses `-` and `_` instead of `+` and `/`.
- Use `strict_encode64` for a value that must stay on one line, such as an HTTP header.
- On the mruby VM, the decode methods return a binary string (ASCII-8BIT), as CRuby does.
  Call `force_encoding("UTF-8")` on the result to read it as text.
- mruby/c strings have no encoding, so there the result is always a plain string.
