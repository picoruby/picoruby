# picoruby-prk-consumer_key

Consumer control keycodes for PRK Firmware - media and system control keys.

## Overview

Provides keycodes for media controls (play, pause, volume) and system functions (sleep, wake).

## Usage

```ruby
require 'keyboard'

kbd = Keyboard.new

# Use consumer keycodes in keymap
kbd.add_layer :media, [
  %i(KC_MUTE KC_VOLU KC_VOLD KC_MPLY),
  %i(KC_MPRV KC_MNXT KC_MSTP KC_EJCT),
  %i(KC_SLEP KC_WAKE KC_PWR  KC_MYCM),
  # ...
]

kbd.start
```

## Common Consumer Keycodes

### Media Controls

- `KC_MUTE` - Mute audio
- `KC_VOLU` - Volume up
- `KC_VOLD` - Volume down
- `KC_MPLY` - Play/Pause
- `KC_MSTP` - Stop
- `KC_MPRV` - Previous track
- `KC_MNXT` - Next track
- `KC_MRWD` - Rewind
- `KC_MFFD` - Fast forward

### System Controls

- `KC_PWR` - Power
- `KC_SLEP` - Sleep
- `KC_WAKE` - Wake
- `KC_EJCT` - Eject

### Application Controls

- `KC_CALC` - Calculator
- `KC_MAIL` - Email
- `KC_MYCM` - My Computer
- `KC_WSCH` - Web Search
- `KC_WHOM` - Web Home
- `KC_WBAK` - Web Back
- `KC_WFWD` - Web Forward
- `KC_WSTP` - Web Stop
- `KC_WREF` - Web Refresh
- `KC_WFAV` - Web Favorites

## Notes

- These are HID Consumer Control codes
- Behavior may vary by operating system
- Useful for media layer on keyboard
- Some keys may require OS support
