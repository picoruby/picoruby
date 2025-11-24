# picoruby-prk-via

VIA protocol support for PRK Firmware - enables runtime keymap configuration with VIA/Vial software.

## Overview

VIA is a protocol that allows real-time keyboard configuration through VIA/Vial desktop applications without reflashing firmware.

## Usage

```ruby
require 'keyboard'

kbd = Keyboard.new

# Initialize VIA support
kbd.init_via

# Define layers (these become configurable in VIA)
kbd.add_layer :default, [
  # Your default keymap
]

kbd.start
```

## Features

- **Runtime Keymap Editing**: Change key assignments without reflashing
- **Layer Management**: Switch between layers
- **RGB Control**: Configure RGB settings
- **Macro Recording**: Create and edit macros
- **Key Testing**: Test individual keys

## VIA/Vial Software

- **VIA**: Official configurator (https://www.caniusevia.com/)
- **Vial**: Open-source fork with additional features (https://get.vial.today/)

## Notes

- Keyboard must be VIA-compatible
- Changes are saved in keyboard EEPROM/flash
- Requires appropriate VIA JSON definition file for full compatibility
- Provides user-friendly GUI alternative to editing Ruby keymap files
- Useful for end-users who don't want to write Ruby code
