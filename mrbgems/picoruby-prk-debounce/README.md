# picoruby-prk-debounce

Debouncing library for PRK Firmware - handles mechanical switch bounce.

## Overview

Provides debouncing algorithms to eliminate false key presses caused by mechanical switch bounce.

## Usage

Debouncing is typically configured in keyboard setup:

```ruby
require 'keyboard'

kbd = Keyboard.new

# Set debounce threshold (milliseconds)
kbd.set_debounce(40)  # Default is 40ms

# Initialize keyboard
kbd.init_matrix_pins(...)
kbd.start
```

## Debounce Strategies

### DebounceBase

Base class with configurable threshold (default: 40ms).

### DebounceNone

For switches that don't bounce mechanically or for gaming applications where minimal latency is critical.

## API

- `threshold=(milliseconds)` - Set debounce threshold
- Default threshold: 40ms
- Minimum threshold: 1ms

## Notes

- Debouncing prevents multiple key events from single press
- Lower threshold = faster response but more false triggers
- Higher threshold = more reliable but slower response
- 40ms is a good balance for most mechanical switches
