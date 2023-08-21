require 'spi'

class MCP3204
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

  def read(channel, differential: false)
    @cs.write 0
    cmd = differential ? 0b100 : 0b110
    res = @spi.transfer cmd|(channel >> 2), (channel & 0b11) << 6, 0
  ensure
    @cs.write 1
    return res
  end
end

