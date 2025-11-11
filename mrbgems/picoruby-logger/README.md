# picoruby-logger

Simple logging library for PicoRuby with buffering and log levels.

## Usage

```ruby
require 'logger'

# Create logger
logger = Logger.new("app.log", level: :info)

# Log messages at different levels
logger.debug("Debug info")     # Won't be logged (below :info)
logger.info("App started")     # Will be logged
logger.warn("Low memory")      # Will be logged
logger.error("Connection lost") # Will be logged
logger.fatal("System crash")   # Will be logged

# Log with block (evaluated only if level is active)
logger.debug { "Expensive operation: #{complex_calculation}" }

# Close logger
logger.close
```

## Log Levels

Levels in order of severity:
- `:debug` - Detailed debugging information
- `:info` - General informational messages
- `:warn` - Warning messages
- `:error` - Error messages
- `:fatal` - Fatal error messages

## API

### Methods

- `Logger.new(io_or_filename, level: :info, buffer_max: 32, trailing_lines: 5)` - Create logger
- `debug(message)` - Log debug message
- `info(message)` - Log info message
- `warn(message)` - Log warning message
- `error(message)` - Log error message
- `fatal(message)` - Log fatal message
- `level=(level)` - Change log level
- `flush_level=(level)` - Set level that triggers immediate flush (default: :warn)
- `flush()` - Flush buffered messages to file
- `close()` - Flush and close logger

## Features

- **Buffering**: Messages are buffered for performance (configurable with `buffer_max`)
- **Auto-flush**: Messages at or above `flush_level` trigger immediate write
- **Trailing lines**: After flush, continues writing N more lines (configurable)
- **Timestamps**: Includes timestamp and uptime in log entries

## Notes

- Logs to both file and STDOUT
- Log format: `timestamp,uptime,level,message`
- Buffer is automatically flushed when full or logger is closed
