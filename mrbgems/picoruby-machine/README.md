# picoruby-machine

Machine/hardware control and system utilities for PicoRuby.

## Usage

```ruby
require 'machine'

# Delays and timing
Machine.delay_ms(100)       # Delay 100ms (allows other tasks)
Machine.busy_wait_ms(10)    # Busy wait 10ms (blocks)
Machine.sleep(1.5)          # Sleep 1.5 seconds

# Deep sleep (wake on GPIO edge)
Machine.deep_sleep(15, true, false)  # Sleep until pin 15 goes low

# System information
id = Machine.unique_id
puts "Unique ID: #{id}"
puts "MCU: #{Machine.mcu_name}"

# Uptime
us = Machine.uptime_us
formatted = Machine.uptime_formatted  # "1d 2h 3m 4s" format

# Hardware clock
Machine.set_hwclock(tv_sec)
sec, nsec = Machine.get_hwclock

# Memory access
data = Machine.read_memory(0x20000000, 256)  # Read 256 bytes

# USB device
if Machine.tud_mounted?
  puts "USB connected"
end
Machine.tud_task  # Process USB tasks

# Exit program
Machine.exit(0)

# Debug output
Machine.debug_puts("Debug message")
```

## API

### Timing Methods

- `Machine.delay_ms(ms)` - Delay with task switching
- `Machine.busy_wait_ms(ms)` - Blocking busy wait
- `Machine.sleep(seconds)` - Sleep (Float or Integer seconds)
- `Machine.deep_sleep(gpio_pin, edge, high)` - Deep sleep until GPIO interrupt

### System Information

- `Machine.unique_id()` - Get unique device ID
- `Machine.mcu_name()` - Get MCU name string
- `Machine.uptime_us()` - Get uptime in microseconds
- `Machine.uptime_formatted()` - Get formatted uptime string

### Hardware Clock

- `Machine.set_hwclock(tv_sec)` - Set hardware clock
- `Machine.get_hwclock()` - Get hardware clock, returns `[sec, nsec]`

### Memory and USB

- `Machine.read_memory(address, size)` - Read from memory address
- `Machine.tud_task()` - Process USB device tasks
- `Machine.tud_mounted?()` - Check if USB is mounted

### Utilities

- `Machine.exit(status)` - Exit program with status code
- `Machine.debug_puts(string)` - Debug output

## Notes

- `delay_ms` allows task switching (use for longer delays)
- `busy_wait_ms` blocks CPU (use only for very short delays)
- Deep sleep saves power but requires hardware wake source
