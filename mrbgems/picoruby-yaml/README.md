# picoruby-yaml

Simple YAML parser and generator library for PicoRuby.

## Usage

```ruby
require 'yaml'

# Parse YAML string
yaml_str = <<~YAML
  name: PicoRuby
  version: 3
  features:
    - small
    - fast
YAML

data = YAML.load(yaml_str)
puts data["name"]  # => "PicoRuby"

# Load YAML from file
config = YAML.load_file("config.yml")

# Generate YAML string
yaml = YAML.dump({name: "PicoRuby", version: 3})
puts yaml
```

## API

### Methods

- `YAML.load(yaml_string)` - Parse YAML string and return Ruby object
- `YAML.load_file(file_path)` - Load and parse YAML file
- `YAML.dump(ruby_object)` - Generate YAML string from Ruby object

## Supported Types

- Hashes, Arrays, Strings, Integers, Floats, Booleans, nil
- Comments (lines starting with `#`)
- Nested structures with indentation

## Notes

- This is a simple implementation, not complete YAML 1.2 spec
- Designed to be small and simple, not fast or feature-complete
- Does not support all YAML features (anchors, tags, etc.)
