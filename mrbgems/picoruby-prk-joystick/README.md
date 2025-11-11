# picoruby-prk-joystick

Analog joystick support for PRK Firmware.

## Usage

```ruby
require 'keyboard'

kbd = Keyboard.new

# Add joystick
kbd.append_joystick(
  26,  # X-axis ADC pin
  27   # Y-axis ADC pin
)

# Configure joystick behavior
kbd.joysticks[0].action_up do
  kbd.macro_send(:KC_UP)
end

kbd.joysticks[0].action_down do
  kbd.macro_send(:KC_DOWN)
end

kbd.joysticks[0].action_left do
  kbd.macro_send(:KC_LEFT)
end

kbd.joysticks[0].action_right do
  kbd.macro_send(:KC_RIGHT)
end

kbd.start
```

## API

### Methods

- `action_up { block }` - Set action for upward movement
- `action_down { block }` - Set action for downward movement
- `action_left { block }` - Set action for leftward movement
- `action_right { block }` - Set action for rightward movement

## Common Uses

### Arrow Keys
```ruby
joystick.action_up    { kbd.macro_send(:KC_UP) }
joystick.action_down  { kbd.macro_send(:KC_DOWN) }
joystick.action_left  { kbd.macro_send(:KC_LEFT) }
joystick.action_right { kbd.macro_send(:KC_RIGHT) }
```

### Mouse Movement
```ruby
joystick.action_up    { kbd.macro_send(:KC_MS_UP) }
joystick.action_down  { kbd.macro_send(:KC_MS_DOWN) }
joystick.action_left  { kbd.macro_send(:KC_MS_LEFT) }
joystick.action_right { kbd.macro_send(:KC_MS_RIGHT) }
```

## Notes

- Requires analog joystick with X and Y outputs
- Uses ADC pins to read joystick position
- Typically includes deadzone to prevent drift
- Threshold detection for directional actions
