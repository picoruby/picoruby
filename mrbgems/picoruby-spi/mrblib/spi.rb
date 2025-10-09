require "gpio"

class SPI
  MSB_FIRST = 1
  LSB_FIRST = 0
  DATA_BITS = 8
  DEFAULT_FREQUENCY = 100_000

  attr_accessor :unit, :cs

  def self.new(unit:, frequency: DEFAULT_FREQUENCY, sck_pin: -1, cipo_pin: -1, copi_pin: -1, cs_pin: -1, mode: 0, first_bit: MSB_FIRST)
    spi = self.init(
      unit.to_s,
      frequency,
      sck_pin,
      cipo_pin,
      copi_pin,
      cs_pin,
      mode,
      first_bit,
      DATA_BITS # Data bit size. No support other than 8
    )
    spi.unit = unit.to_s
    if -1 < cs_pin && !spi.unit.start_with?("ESP32")
      cs = GPIO.new(cs_pin, GPIO::OUT)
      cs.write(1)
      spi.cs = cs
    end
    return spi
  end

  def select
    @cs&.write 0
    if block_given?
      begin
        yield self
      ensure
        deselect
      end
    end
  end

  def deselect
    @cs&.write 1
  end

end
