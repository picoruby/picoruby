# picoruby-prk-rotary_encoder

Rotary encoder support for PRK Firmware.

## Usage

```ruby
require 'keyboard'

kbd = Keyboard.new

# Add rotary encoder
kbd.append_rotary_encoder(18, 19)  # pin_a, pin_b

# Configure encoder behavior
kbd.rotary_encoders[0].clockwise do
  kbd.macro_send(:KC_VOLU)  # Volume up
end

kbd.rotary_encoders[0].counterclockwise do
  kbd.macro_send(:KC_VOLD)  # Volume down
end

# For split keyboards
kbd.rotary_encoders[0].configure(:left)   # This encoder is on left half
kbd.rotary_encoders[1].configure(:right)  # This encoder is on right half

kbd.start
```

## API

### RotaryEncoder Methods

- `clockwise { block }` / `cw { block }` - Set clockwise rotation action
- `counterclockwise { block }` / `ccw { block }` - Set counter-clockwise rotation action
- `configure(side)` - Configure for split keyboard (`:left` or `:right`)
- `left?()` - Check if encoder is on left side
- `right?()` - Check if encoder is on right side

## Common Uses

### Volume Control
```ruby
encoder.cw { kbd.macro_send(:KC_VOLU) }
encoder.ccw { kbd.macro_send(:KC_VOLD) }
```

### Scrolling
```ruby
encoder.cw { kbd.macro_send(:KC_MS_WH_UP) }
encoder.ccw { kbd.macro_send(:KC_MS_WH_DOWN) }
```

### Layer Switching
```ruby
encoder.cw { kbd.layer_up }
encoder.ccw { kbd.layer_down }
```

## Notes

- Requires two GPIO pins per encoder (A and B phases)
- Multiple encoders can be added
- Detent-based encoders recommended
- Works with split keyboards
