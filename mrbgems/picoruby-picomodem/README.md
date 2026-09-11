# picoruby-picomodem

R2P2 Binary Transfer Protocol (RBTP) implementation for PicoRuby.

Enables machine-to-machine communication for file transfers, firmware updates (DFU), and other device operations.

## Usage

### Device side

```ruby
require 'picomodem'

# Run a PicoModem session
PicoModem.session($stdin, $stdout)
```

### Host side (command-line client)

`tools/picomodem.rb` is a terminal client that drives a device whose
shell speaks PicoModem (R2P2's shell, or Nicht's haush) over its USB
CDC console. It runs on the host `picoruby` build. Put the device at
its prompt first; the client sends Ctrl-B to enter transfer mode.

```
picoruby tools/picomodem.rb [-d DEVICE] [-v] put LOCAL [REMOTE]
picoruby tools/picomodem.rb [-d DEVICE] [-v] get REMOTE [LOCAL]
```

`DEVICE` defaults to `/dev/ttyACM0`. `-v` traces the exchange on
stderr. Both directions are checked end to end with CRC-32.

## Features

- File download/upload over serial connection
- Firmware DFU via binary transfer protocol
- Incremental CRC32 for large file transfers
- Robust timeout handling

## License

MIT
