# picoruby-toml

TOML parser and generator for PicoRuby.

This is a pure Ruby implementation of TOML for PicoRuby, designed to be small and simple.

## Features

- Parse TOML documents
- Generate TOML documents from Ruby objects
- Support for all TOML data types
- Pure Ruby implementation

## Usage

### Parse TOML

```ruby
require 'toml'

toml_str = <<~TOML
  title = "TOML Example"

  [owner]
  name = "Tom Preston-Werner"
  dob = 1979-05-27T07:32:00-08:00

  [database]
  enabled = true
  ports = [8001, 8002, 8003]
TOML

data = TOML.parse(toml_str)
puts data["title"]  # "TOML Example"
puts data["owner"]["name"]  # "Tom Preston-Werner"
```

### Generate TOML

```ruby
require 'toml'

data = {
  "title" => "Config",
  "server" => {
    "host" => "localhost",
    "port" => 8080
  }
}

toml_str = TOML.generate(data)
```

## Supported Types

- String (basic, literal, multi-line)
- Integer
- Float
- Boolean
- Date-Time (basic support)
- Array
- Table (Hash)

## License

MIT
