# picoruby-json

JSON parser and generator library for PicoRuby.

## Usage

```ruby
require 'json'

# Parse JSON string
data = JSON.parse('{"name":"PicoRuby","version":1}')
puts data["name"]  # => "PicoRuby"

# Generate JSON string
json = JSON.generate({name: "PicoRuby", values: [1, 2, 3]})
puts json  # => '{"name":"PicoRuby","values":[1,2,3]}'

# Dig into nested JSON without full parsing
digger = JSON::Digger.new('{"user":{"name":"Alice"}}')
result = digger.dig("user", "name").parse
puts result  # => "Alice"
```

## API

### Module Methods

- `JSON.parse(string)` - Parse JSON string and return Ruby object
- `JSON.generate(object)` - Generate JSON string from Ruby object

### JSON::Digger

- `JSON::Digger.new(json_string)` - Create digger for efficient partial parsing
- `dig(*keys)` - Navigate to specific path in JSON
- `parse()` - Parse the value at current path

## Supported Types

- **JSON → Ruby**: Object → Hash, Array → Array, String → String, Number → Integer/Float, true/false → TrueClass/FalseClass, null → nil
- **Ruby → JSON**: Hash, Array, String, Integer, Float, TrueClass, FalseClass, NilClass

## Notes

- Use `JSON::Digger` for efficiently extracting specific values from large JSON without parsing everything
