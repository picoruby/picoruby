# picoruby-base_encoding

Base45, Base58, and Base62 encoding/decoding for PicoRuby.

This is a pure Ruby implementation of various base encoding schemes for PicoRuby.

## Features

- Base58 - Used in Bitcoin addresses (no 0, O, I, l)
- Base62 - Alphanumeric encoding (a-z, A-Z, 0-9)
- Base45 - QR code friendly encoding

## Usage

### Base58

```ruby
require 'base_encoding'

# Encode
encoded = Base58.encode("Hello, World!")
# => "72k1xXWG59fYdzXN"

# Decode
decoded = Base58.decode("72k1xXWG59fYdzXN")
# => "Hello, World!"

# Bitcoin alphabet (default)
Base58.encode("test", alphabet: :bitcoin)

# Flickr alphabet
Base58.encode("test", alphabet: :flickr)
```

### Base62

```ruby
require 'base_encoding'

# Encode
encoded = Base62.encode("Hello, World!")
# => "T8dgcjRGuYUueWht"

# Decode
decoded = Base62.decode("T8dgcjRGuYUueWht")
# => "Hello, World!"
```

### Base45

```ruby
require 'base_encoding'

# Encode
encoded = Base45.encode("Hello, World!")
# => "6D3E9E8FEE3F0A2F8"

# Decode
decoded = Base45.decode("6D3E9E8FEE3F0A2F8")
# => "Hello, World!"
```

## Use Cases

- **Base58**: Bitcoin addresses, IPFS hashes, short URLs
- **Base62**: URL-safe identifiers, database IDs
- **Base45**: QR codes, EU Digital COVID Certificate

## License

MIT
