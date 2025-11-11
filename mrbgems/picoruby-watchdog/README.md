# picoruby-watchdog

Watchdog timer for PicoRuby - automatic reset if system hangs.

## Usage

```ruby
require 'watchdog'

# Enable watchdog with 5 second timeout
Watchdog.enable(5000)  # 5000ms = 5 seconds

# Main loop
loop do
  # Do work...

  # Feed watchdog to prevent reset
  Watchdog.feed

  sleep 1
end

# Disable watchdog
Watchdog.disable

# Check if watchdog caused last reboot
if Watchdog.caused_reboot?
  puts "System recovered from hang"
end
```

## API

### Methods

- `Watchdog.enable(delay_ms, pause_on_debug: false)` - Enable watchdog
  - System will reset if not fed within `delay_ms` milliseconds
  - `pause_on_debug`: Pause watchdog when debugger is attached
- `Watchdog.disable()` - Disable watchdog
- `Watchdog.feed()` - Feed watchdog (reset timer)
- `Watchdog.update(delay_ms)` - Update watchdog timeout
- `Watchdog.reboot(delay_ms)` - Trigger reboot after delay
- `Watchdog.caused_reboot?()` - Check if watchdog caused last reboot
- `Watchdog.enable_caused_reboot?()` - Check if enabled watchdog caused reboot
- `Watchdog.get_count()` - Get watchdog counter
- `Watchdog.start_tick(cycle)` - Start watchdog tick

### Constants

- `RP2040_MAX_ENABLE_MS` - Maximum enable time for RP2040 (platform-specific)

## Use Cases

- **System Reliability**: Auto-recover from software hangs
- **Embedded Systems**: Ensure long-running devices stay operational
- **Error Recovery**: Automatic reset on deadlock

## Notes

- Must call `feed()` periodically before timeout expires
- Typical timeout: 1-10 seconds depending on application
- RP2040 has maximum timeout limit
- Useful for unattended embedded systems
- Cannot be disabled once triggered reboot starts
