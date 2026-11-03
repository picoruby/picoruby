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

## nRF52 notes

- Assignments persist to `/etc/env` on the littlefs volume, so they survive
  a reboot. The file is a flat `KEY=VALUE` per line and is rewritten whole
  on every change.
- `ENV` itself is still an in-RAM hash; the file is read once at boot to
  populate it. A key written by another program while this one is running
  is not seen until the next boot.
- Keys are limited to 64 bytes, values to 192, and the whole file to 512.
  A key or value containing a newline, or a key containing `=`, is
  rejected rather than written in a form that could not be read back.
- `TZ` is parsed as it is loaded, so a timezone set in a previous session
  applies to `Time` from boot.
