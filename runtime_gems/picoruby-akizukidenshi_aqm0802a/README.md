# picoruby-akizukidenshi_aqm0802a

AQM0802A 8x2 character LCD display library for PicoRuby.

## Usage

```ruby
require 'aqm0802a'

i2c = I2C.new(unit: :RP2040_I2C0, sda_pin: 14, scl_pin: 15)
lcd = AQM0802A.new(i2c: i2c)

lcd.print "Hello!"
lcd.break_line
lcd.print "PicoRuby"
```

## API

### Methods

- `AQM0802A.new(i2c:, init: true)` - Initialize LCD with I2C instance
- `print(text)` - Print text to LCD
- `putc(char)` - Print single character
- `break_line()` - Move to second line
- `clear()` - Clear display
- `home()` - Move cursor to home position
- `reset()` - Reset and initialize LCD
- `write_instruction(inst)` - Write instruction byte to LCD

## Notes

- I2C address is 0x3E (fixed)
- Display size: 8 characters × 2 lines
- Supports both AQM0802A-RN-GBW (no backlight) and AQM0802A-FLW-GBW (with backlight)
