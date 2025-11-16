# picoruby-cbor

CBOR (Concise Binary Object Representation) encoder and decoder for PicoRuby.

This is a pure Ruby implementation of CBOR (RFC 8949) for PicoRuby, designed for IoT devices and constrained networks.

## Features

- CBOR encoder and decoder
- Support for all major CBOR data types
- Tagged values (timestamps, etc.)
- Deterministic encoding option
- Pure Ruby implementation

## Usage

### Encode

```ruby
require 'cbor'

# Basic types
CBOR.encode(42)              # Integer
CBOR.encode(3.14)            # Float
CBOR.encode("hello")         # String
CBOR.encode(true)            # Boolean
CBOR.encode(nil)             # Null

# Collections
CBOR.encode([1, 2, 3])       # Array
CBOR.encode({a: 1, b: 2})    # Map/Hash

# Nested structures
data = {
  name: "sensor",
  values: [20.5, 21.0, 19.8],
  active: true
}
encoded = CBOR.encode(data)
```

### Decode

```ruby
require 'cbor'

# Decode CBOR data
decoded = CBOR.decode(encoded_data)
```

### Tagged Values

```ruby
# Encode with tag (e.g., timestamp)
time = Time.now
tagged = CBOR::Tagged.new(1, time.to_i)  # Tag 1 = epoch timestamp
encoded = CBOR.encode(tagged)

# Decode tagged value
decoded = CBOR.decode(encoded)
# => CBOR::Tagged instance
```

## Supported Types

- Unsigned integers (0 to 2^64-1)
- Negative integers
- Byte strings (binary data)
- Text strings (UTF-8)
- Arrays
- Maps (hashes)
- Boolean (true/false)
- Null and undefined
- Floating-point (32/64-bit)
- Tagged values (timestamps, etc.)

## Major Types

CBOR uses major types to categorize data:

- Type 0: Unsigned integer
- Type 1: Negative integer
- Type 2: Byte string
- Type 3: Text string
- Type 4: Array
- Type 5: Map
- Type 6: Tagged value
- Type 7: Simple values and floats

## Use Cases

- IoT sensor data transmission
- CoAP payloads
- Efficient data storage
- Protocol buffers alternative
- Configuration files

## License

MIT
