require "time"

class PCF8523
  ADDRESS = 0x68

  def initialize(i2c:)
    @i2c = i2c
    # Reset
    @i2c.write(ADDRESS, 0x00, 0x58)
    # Reset command set power management default
    # so we enable switch-over everytime
    set_power_management
  end

  attr_reader :i2c # for debug. eg) $rtc.i2c.read(0x68, 3, 0)

  #
  # (from https://www.nxp.com/docs/en/data-sheet/PCF8523.pdf)
  #
  # Power management function
  #
  # pow =
  # 0b000     battery switch-over function is enabled in standard mode;
  #           battery low detection function is enabled
  # 0b001     battery switch-over function is enabled in direct switching mode;
  #           battery low detection function is enabled
  # 0b010,011 battery switch-over function is disabled - only one power supply (VDD);
  #           battery low detection function is enabled
  # 0b100     battery switch-over function is enabled in standard mode;
  #           battery low detection function is disabled
  # 0b101     battery switch-over function is enabled in direct switching mode;
  #           battery low detection function is disabled
  # 0b110     not allowed
  # 0b111     *Default
  #           battery switch-over function is disabled - only one power supply (VDD);
  #           battery low detection function is disabled
  #
  # Default 02h register: 111-0000 (`-`: not implemented)
  #                       ^^^
  #                       pow
  #
  # Direct switching mode is suitable for 5V Vdd
  def set_power_management(pow = 0b001)
    @i2c.write(ADDRESS, 2, pow << 5)
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

