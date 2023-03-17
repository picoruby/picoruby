require "time"
require "i2c"

class PCF8523
  ADDRESS = 0x68

  def initialize(unit: nil, sda_pin:, scl_pin:, frequency: 400_000)
    @i2c = I2C.new(
      unit: unit,
      sda_pin: sda_pin,
      scl_pin: scl_pin,
      frequency: frequency
    )
    # Reset
    @i2c.write(ADDRESS, 0x00, 0x58)
  end

  def current_time=(time)
    yy = time.year - 2000
    if yy < 0 || 99 < yy
      raise ArgumentError.new("out of range :year. (expected 2000..2099)")
    end
    [
      ((time.sec  / 10) << 4) | (time.sec  % 10),
      ((time.min  / 10) << 4) | (time.min  % 10),
      ((time.hour / 10) << 4) | (time.hour % 10),
      ((time.mday / 10) << 4) | (time.mday % 10),
      time.wday,
      ((time.mon  / 10) << 4) | (time.mon  % 10),
      ((yy        / 10) << 4) | (yy        % 10),
    ].each_with_index do |value, i|
      @i2c.write(ADDRESS, i + 3, value)
    end
    time
  end

  def current_time
    raw_time = @i2c.read(ADDRESS, 7, 0x03).bytes
    Time.local(
      (10 * ((raw_time[6] & 0xF0) >> 4)) + (raw_time[6] & 0x0F) + 2000, # year
      (10 * ((raw_time[5] & 0x10) >> 4)) + (raw_time[5] & 0x0F),        # mon
      #                                     raw_time[4] is wday
      (10 * ((raw_time[3] & 0x30) >> 4)) + (raw_time[3] & 0x0F),        # mday
      (10 * ((raw_time[2] & 0x30) >> 4)) + (raw_time[2] & 0x0F),        # hour
      (10 * ((raw_time[1] & 0x70) >> 4)) + (raw_time[1] & 0x0F),        # min
      (10 * ((raw_time[0] & 0x70) >> 4)) + (raw_time[0] & 0x0F)         # sec
    )
  end
end

