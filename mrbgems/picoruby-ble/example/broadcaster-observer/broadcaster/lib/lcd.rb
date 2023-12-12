require 'i2c'

class LCD

  ADDRESS = 0x3e # 0x7c == (0x3e << 1) + 0 (R/W)

  def initialize(i2c:, init: true)
    @i2c = i2c
    reset if init
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
