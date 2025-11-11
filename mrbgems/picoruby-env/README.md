# picoruby-env

Environment variables management for PicoRuby.

## Usage

```ruby
require 'env'

# Get environment variable
home = ENV["HOME"]
puts home

# Set environment variable
ENV["MY_VAR"] = "value"

# Delete environment variable
ENV.delete("MY_VAR")

# With block (called if key doesn't exist)
ENV.delete("MY_VAR") { |key| puts "#{key} not found" }

# Iterate over all variables
ENV.each do |key, value|
  puts "#{key}=#{value}"
end
```

## API

### Methods

- `ENV[key]` - Get environment variable value
- `ENV[key] = value` - Set environment variable
- `ENV.delete(key)` - Delete environment variable, returns old value or nil
- `ENV.delete(key) { }` - Delete with block, called if key doesn't exist
- `ENV.each { |key, value| }` - Iterate over all environment variables

## Predefined Constants

- `ENV_DEFAULT_WIFI_CONFIG_PATH` - Default WiFi configuration file path
- `ENV_DEFAULT_HOME` - Default home directory path

## Notes

- Environment variables are stored in memory
- Commonly used variables: `HOME`, `PWD`, `PATH`
- VFS uses `PWD` to track current working directory
