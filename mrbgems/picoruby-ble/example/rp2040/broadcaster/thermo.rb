require 'spi'

# Usage:
#  require 'thermo'
#  thermo = THERMO.new(unit: :RP2040_SPI0, cipo_pin: 16, cs_pin: 17, sck_pin: 18, copi_pin: 19)
#  p thermo.read
class THERMO
  def initialize(unit:, sck_pin:, cipo_pin:, copi_pin:, cs_pin:)
    @spi = SPI.new(unit: unit, frequency: 500_000, mode: 0, cs_pin: cs_pin,
      sck_pin: sck_pin, cipo_pin: cipo_pin, copi_pin: copi_pin
    )
    @spi.select
    @spi.write 0xFF, 0xFF, 0xFF, 0xFF # reset
    @spi.write 0x54 # continuous read mode
    sleep_ms 240
  end

  def read
    data = @spi.read(2).bytes
    temp = (data[0] << 8 | data[1]) >> 3
    # If it minus?
    temp -= 0x2000 if 0 < temp & 0b1_0000_0000_0000
    temp / 16.0  # Convert to Celsius
  end
end
