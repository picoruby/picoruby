# picoruby-prk-rgb

RGB LED control library for PRK Firmware - supports underglow and per-key backlighting.

## Usage

```ruby
require 'keyboard'

kbd = Keyboard.new

# Initialize RGB
kbd.init_rgb(
  pin: 25,
  underglow: 10,  # Number of underglow LEDs
  backlight: 60,  # Number of backlight LEDs
  is_rgbw: false  # Set true for RGBW LEDs
)

# RGB is controlled via keycodes in your keymap
kbd.add_layer :default, [
  %i(KC_Q RGB_TOG RGB_MOD RGB_HUI),
  # ...
]

kbd.start
```

## RGB Keycodes

- `RGB_TOG` - Toggle RGB on/off
- `RGB_MOD` / `RGB_MODE_FORWARD` - Next RGB mode
- `RGB_RMOD` / `RGB_MODE_REVERSE` - Previous RGB mode
- `RGB_HUI` - Increase hue
- `RGB_HUD` - Decrease hue
- `RGB_SAI` - Increase saturation
- `RGB_SAD` - Decrease saturation
- `RGB_VAI` - Increase brightness
- `RGB_VAD` - Decrease brightness
- `RGB_SPI` - Increase speed
- `RGB_SPD` - Decrease speed

## Features

- Multiple RGB animation modes
- Separate underglow and backlight control
- HSV color space control
- Adjustable speed and brightness
- Support for both RGB and RGBW LEDs

## Notes

- Uses WS2812(B) or SK6812 addressable LEDs
- Requires RMT peripheral for precise timing
- Underglow: LEDs around keyboard perimeter
- Backlight: LEDs under each key
