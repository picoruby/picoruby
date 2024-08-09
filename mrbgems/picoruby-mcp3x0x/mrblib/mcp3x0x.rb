require 'spi'

# MCP3D0C
#      ^
#      0: SPI. This gem supports.
#
# MCP3D2C
#      ^
#      2: I2C. Not supported. Use MCP3x2x instead.
#
# D: Depth. 0: 10bit, 2: 12bit, 3: 13bit, 4: 16bit or 18bit, 5: 22bit
# C: Number of channels variation.

# Usage:
#   adc = MCP3004.new(unit: :RP2040_SPI0, sck_pin: 18, cipo_pin: 16, copi_pin: 19, cs_pin: 17)
#   adc.read(0)  # Read channel 0

class MCP3x0x
  DEFAULT_FREQUENCY = 500_000

  def initialize(unit:, frequency: DEFAULT_FREQUENCY, sck_pin: -1, cipo_pin: -1, copi_pin: -1, cs_pin: -1)
    @spi = SPI.new(
      unit: unit,
      frequency: frequency,
      sck_pin: sck_pin,
      cipo_pin: cipo_pin,
      copi_pin: copi_pin,
      mode: 0
    )
    @cs = GPIO.new(cs_pin, GPIO::OUT)
    @cs.write 1
  end
end

class MCP3008 < MCP3x0x
  # channel: 0..7
  # When differential is true, channel parameter indicates a pseudo-differential:
  #   0: CH0(+) - CH1(-)
  #   1: CH0(-) - CH1(+)
  #   2: CH2(+) - CH3(-)
  #   3: CH2(-) - CH3(+)
  #   4: CH4(+) - CH5(-)
  #   5: CH4(-) - CH5(+)
  #   6: CH6(+) - CH7(-)
  #   7: CH6(-) - CH7(+)
  def read(channel, differential: false)
    @cs.write 0
    cmd = differential ? 0 : 0b10000000
    _, d1, d2 = @spi.transfer(1, cmd|(channel << 4), 0).bytes
    @cs.write 1
    return ((d1 || 0) & 0b11) << 8 | (d2 || 0)
  end
end

class MCP3004 < MCP3008
  # channel: 0..3
  # When differential is true, channel parameter indicates a pseudo-differential:
  #   0: CH0(+) - CH1(-)
  #   1: CH0(-) - CH1(+)
  #   2: CH2(+) - CH3(-)
  #   3: CH2(-) - CH3(+)
end

class MCP3208 < MCP3x0x
  # channel: 0..7
  # When differential is true, channel parameter indicates a pseudo-differential:
  #   0: CH0(+) - CH1(-)
  #   1: CH0(-) - CH1(+)
  #   2: CH2(+) - CH3(-)
  #   3: CH2(-) - CH3(+)
  #   4: CH4(+) - CH5(-)
  #   5: CH4(-) - CH5(+)
  #   6: CH6(+) - CH7(-)
  #   7: CH6(-) - CH7(+)
  def read(channel, differential: false)
    @cs.write 0
    cmd = differential ? 0b100 : 0b110
    _, d1, d2 = @spi.transfer(cmd|((channel >> 2)&1), (channel & 0b11) << 6, 0).bytes
    @cs.write 1
    return ((d1 || 0) & 0b1111) << 8 | (d2 || 0)
  end
end

class MCP3204 < MCP3x0x
  # channel: 0..3
  # When differential is true, channel parameter indicates a pseudo-differential:
  #   0: CH0(+) - CH1(-)
  #   1: CH0(-) - CH1(+)
  #   2: CH2(+) - CH3(-)
  #   3: CH2(-) - CH3(+)
  def read(channel, differential: false)
    @cs.write 0
    cmd = differential ? 0b100 : 0b110
    _, d1, d2 = @spi.transfer(cmd, (channel & 0b11) << 6, 0).bytes
    @cs.write 1
    return ((d1 || 0) & 0b1111) << 8 | (d2 || 0)
  end
end

