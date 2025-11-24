# picoruby-jwt

JSON Web Token (JWT) encoding and decoding for PicoRuby.

## Overview

Create and verify JWTs for authentication and secure data exchange.

## Usage

```ruby
require 'jwt'

# Create payload
payload = {
  user_id: 123,
  username: "alice",
  exp: Time.now.to_i + 3600  # Expires in 1 hour
}

# Encode JWT with secret
secret = "your-secret-key"
token = JWT.encode(payload, secret)
puts token

# Decode and verify JWT
begin
  decoded = JWT.decode(token, secret)
  puts "User ID: #{decoded['user_id']}"
  puts "Username: #{decoded['username']}"
rescue JWT::VerificationError
  puts "Invalid token signature"
rescue JWT::ExpiredSignature
  puts "Token has expired"
end
```

## API

### Methods

- `JWT.encode(payload, secret)` - Encode payload to JWT string
  - `payload`: Hash to encode
  - `secret`: Secret key for signing
  - Returns: JWT token string

- `JWT.decode(token, secret)` - Decode and verify JWT
  - `token`: JWT token string
  - `secret`: Secret key for verification
  - Returns: Decoded payload hash
  - Raises: `JWT::VerificationError`, `JWT::ExpiredSignature`

## JWT Structure

A JWT consists of three parts separated by dots:
```
header.payload.signature
```

Example:
```
eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VyX2lkIjoxMjN9.signature
```

## Common Use Cases

- API authentication
- Single sign-on (SSO)
- Secure data exchange
- Stateless sessions

## Notes

- Uses HMAC-SHA256 for signing (HS256)
- Tokens are not encrypted (payload is base64-encoded)
- Always use HTTPS when transmitting JWTs
- Keep secret keys secure
- Set expiration times to limit token lifetime
- Requires `json` and `base64` libraries
