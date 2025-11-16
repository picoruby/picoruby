# picoruby-msgpack

MessagePack serializer/deserializer for PicoRuby.

This is a pure Ruby implementation of MessagePack for PicoRuby, designed to be small and simple.

## Usage

```ruby
# Pack (serialize)
data = { name: "Alice", age: 30, items: [1, 2, 3] }
packed = MessagePack.pack(data)

# Unpack (deserialize)
unpacked = MessagePack.unpack(packed)
```

## Supported Types

- nil
- Boolean (true/false)
- Integer
- Float
- String
- Array
- Hash

## License

MIT
