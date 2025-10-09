# Example of ADNS-5050 Optical Mouse Sensor with SPI interface
# https://www.espruino.com/datasheets/ADNS5050.pdf

require "spi"

class ADNS5050 < SPI
  CPI = [ nil, 125, 250, 375, 500, 625, 750, 875, 1000, 1125, 1250, 1375 ]
  DEFAULT_CPI = 1250

  def get_cpi
    self.select do |spi|
      spi.write(0x19)
      reg = spi.read(1).bytes[0] & 0b1111
      CPI[reg]
    end
  end

  def set_cpi(cpi)
    index = CPI.index(cpi) || DEFAULT_CPI
    self.select do |spi|
      spi.write(0x19 | 0x80, index | 0b10000)
    end
    puts "CPI: #{CPI[index]}"
  end

  def reset_chip
    self.select do |spi|
      spi.write(0x3a | 0x80, 0x5a)
    end
    sleep_ms 10
    puts "ADNS-5050 power UP"
  end

end


adns = ADNS5050.new(unit: :BITBANG, sck_pin: 26, copi_pin: 27, cipo_pin: 27, cs_pin: 22)
adns.reset_chip
adns.set_cpi(1250)
while true
  adns.select do |spi|
    spi.write(0x63) # Motion_Burst
    y, x = spi.read(2).bytes
    puts "X: #{x}, Y: #{y}"
  end
  sleep_ms 100
end
