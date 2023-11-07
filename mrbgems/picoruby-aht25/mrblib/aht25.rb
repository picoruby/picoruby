require 'i2c'

class AHT25
  ADDRESS = 0x38

  def initialize(i2c:)
    @i2c = i2c
    sleep_ms 100
    check
  end

  def check
    while true
      @i2c.write(ADDRESS, 0x71)
      status = @i2c.read(ADDRESS, 1).bytes[0]
      break if status & 0x18 == 0x18
      [0x1b, 0x1c, 0x1e].each do |reg|
        @i2c.write(ADDRESS, reg, 0x71)
        sleep_ms 10
      end
      sleep_ms 100
    end
    sleep_ms 10
  end

  def read
    @i2c.write(ADDRESS, 0xAC, 0x33, 0x00)
    sleep_ms 80
    data = [0]
    while data[0] & 0b10000000 == 0
      data = @i2c.read(ADDRESS, 7).bytes # data[6] is checksum
    end
    hum = data[1] << 12 | data[2] << 4 | ((data[3] & 0xF0) >> 4)
    tmp = ((data[3] & 0x0F) << 16) | data[4] << 8 | data[5]
    return({
      temperature: tmp.to_f / 2**20 * 200 - 50,
      humidity:    hum.to_f / 2**20 * 100
    })
  end
end
