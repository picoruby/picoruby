# picoruby-machine

Machine/hardware control and system utilities for PicoRuby.

## Usage

```ruby
require 'machine'

# Delays and timing
Machine.delay_ms(100)       # Delay 100ms (allows other tasks)
Machine.busy_wait_ms(10)    # Busy wait 10ms (blocks)

# Low-power sleep (see "Low-power sleep" below)
Machine.sleep(deep: true, source: :timer, ms: 10_000)
Machine.sleep(deep: true, source: pin, level: GPIO::EDGE_FALL)

# System information
id = Machine.unique_id
puts "Unique ID: #{id}"

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
- `Machine.sleep(deep:, source:, ms:/level:)` - Low-power sleep; see below

### System Information

- `Machine.unique_id()` - Get unique device ID
- `Machine.posix? - Return true if the plarform is a POSIX
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

## Low-power sleep

`Machine.sleep` stops the WHOLE machine -- every task, the millisecond
tick, delivery of every kind -- until the wake source fires, then
execution continues right after the call. It never reboots. This is
machine-level sleep; `Kernel#sleep` is task-level waiting and a
different thing entirely.

Two orthogonal keywords cover the whole matrix:

```ruby
Machine.sleep(deep: false, source: :timer, ms: 5000)   # SLEEP,   wake by timer
Machine.sleep(deep: true,  source: :timer, ms: 10_000) # DORMANT, wake by timer
Machine.sleep(deep: false, source: pin, level: GPIO::LEVEL_LOW)  # SLEEP,   wake by GPIO
Machine.sleep(deep: true,  source: pin, level: GPIO::EDGE_FALL)  # DORMANT, wake by GPIO
```

`deep: false` is SLEEP mode: clocks are gated but oscillators keep
running, USB stays clocked (the console survives), and wake is fast.
`deep: true` is DORMANT: the oscillators themselves stop, power drops
by an order of magnitude, and USB is torn down and re-initialized
around the sleep -- the host re-enumerates the device, so a terminal
session may need reopening.

`source:` is `:timer` with `ms:` (1 to 4294967295, about 49.7 days),
or any object with a `#pin` method -- normally a `GPIO` instance --
with `level:`, one of `GPIO::LEVEL_LOW`, `LEVEL_HIGH`, `EDGE_FALL`,
`EDGE_RISE` (defined by the picoruby-irq gem; combined masks are not
accepted). Returns nil, raises on anything it cannot do:
`ArgumentError` for bad arguments, `NotImplementedError` where the
mode does not exist, `RuntimeError` when hardware state prevents
sleeping.

Reference power draw, from the SDK's timer-wake measurements on
Pico-series boards (GPIO wake, the always-on clock and a connected
USB host can all draw more):

| Mode    | Pico (RP2040) | Pico 2 (RP2350) |
|---------|---------------|-----------------|
| SLEEP   | ~7.3mA        | ~5.9mA          |
| DORMANT | ~0.95mA       | ~3.3mA          |

Rules and fine print:

- **Timed DORMANT has a minimum**: 2000ms on RP2040 (the RTC ticks in
  whole seconds), 10ms on RP2350. Below it raises `ArgumentError`.
- **`Time.now` survives every cell.** The SLEEP cells keep the system
  timer running; the DORMANT cells carry time across on the
  always-on timer and re-sync on wake. On RP2040 that carry is
  quantized to 1 second.
- **GPIO handlers registered through `GPIO#irq` survive a GPIO-wake
  sleep** and fire for events after it. During the sleep they are
  offline: EDGE events on other pins are discarded, and the wake
  event itself is not delivered to handlers. A LEVEL condition still
  asserted at resume fires its handler immediately -- level is a
  condition, not an event.
- **Do not sleep while core1 is running** (e.g. PSG playback): the
  clock reconfiguration is not synchronized with core1. Stop it
  first.
- **ESP32** supports only `deep: false, source: :timer` (light
  sleep); everything else raises `NotImplementedError` there. On
  POSIX the timer cells are a plain sleep and GPIO wake raises.

## Notes

- `delay_ms` allows task switching (use for longer delays)
- `busy_wait_ms` blocks CPU (use only for very short delays)
