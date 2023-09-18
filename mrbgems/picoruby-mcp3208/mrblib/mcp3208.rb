require 'spi'

class MCP3208
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

class MCP3204 < MCP3208
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

