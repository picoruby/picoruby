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
    #@spi.write cmd|(channel >> 2), (channel & 0x11) << 6, 0
    @spi.write cmd|(channel >> 2)
    @spi.read 1
    @spi.write (channel & 0x11) << 6
    hi = @spi.read 1
    @spi.write 0
    lo = @spi.read 1
  ensure
    @cs.write 1
    return hi+lo
  end
end

