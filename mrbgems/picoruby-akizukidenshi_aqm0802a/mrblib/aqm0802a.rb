require 'i2c'

# AQM0802A-RN-GBW: without backlight
# AQM0802A-FLW-GBW: with backlight
# datasheet: https://akizukidenshi.com/download/ds/xiamen/AQM0802.pdf

# Usage:
#
# require 'aqm0802a'
# GPIO.new(3, GPIO::OUT).write 1 # lcd_vdd if needed
# lcd = AQM0802A.new(i2c: I2C.new(unit: :RP2040_I2C1, sda_pin: 14, scl_pin: 15))
# lcd.print "Hello!"
# lcd.break_line
# lcd.print "PicoRuby"

class AQM0802A

  ADDRESS = 0x3e # 0x7c == (0x3e << 1) + 0 (R/W)

  def initialize(i2c:)
    @i2c = i2c
    reset
  end

  def reset
    [ 0x38,     # Function set
      0x39,     # Function set
      0x14,     # Internal OSC frequency
      0x70,     # Contrast set
      0x56,     # Power/ICON/Contrast set
      0x6c      # Follower control
    ].each { |i| write_instruction(i) }
    sleep_ms 200
    [ 0x38,     # Function set
      0x0c,     # Display ON/OFF control
      0x01      # Clear Display
    ].each { |i| write_instruction(i) }
    self
  end

  def write_instruction(inst)
    @i2c.write(ADDRESS, 0, inst)
    sleep_ms 1
    self
  end

  def clear
    write_instruction(0x01)
  end

  def home
    write_instruction(0x02)
  end

  def break_line
    write_instruction(0x80|0x40)
  end

  def putc(c)
    @i2c.write(ADDRESS, 0x40, c)
    sleep_ms 1
    c
  end

  def print(line)
    line.bytes.each { |c| putc c }
    nil
  end

end

