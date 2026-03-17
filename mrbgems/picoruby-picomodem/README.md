# picoruby-picomodem

R2P2 Binary Transfer Protocol (RBTP) implementation for PicoRuby.

Enables machine-to-machine communication for file transfers, firmware updates (DFU), and other device operations.

## Usage

```ruby
require 'picomodem'

# Run a PicoModem session
PicoModem.session($stdin, $stdout)
```

## Features

- File download/upload over serial connection
- Firmware DFU via binary transfer protocol
- Incremental CRC32 for large file transfers
- Robust timeout handling

## License

MIT
