require 'i2c'

# AQM0802A-RN-GBW: without backlight
# AQM0802A-FLW-GBW: with backlight
# datasheet: https://akizukidenshi.com/download/ds/xiamen/AQM0802.pdf

class AQM0802A
  ADDRESS = 0x7c
  def initialize(i2c:)
    @i2c = i2c
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
  end

  def write_instruction(inst)
    @i2c.write(ADDRESS, 0, inst)
    sleep_ms 1
    self
  end

  def write_data(*data)
    @i2c.write(ADDRESS, 0x40, *data)
    sleep_ms 1
    self
  end

  def clear
    write_instruction(0x01)
  end

  def home
    write_instruction(0x02)
  end

  def put_line(line)
    line.bytes.each do |c|
      write_data(c)
    end
  end

  def break_line
    write_instruction(0x80|0x40)
  end
end

