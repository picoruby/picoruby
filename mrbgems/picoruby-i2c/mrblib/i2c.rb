require "gpio"

class I2C
  DEFAULT_FREQUENCY = 100_000 # Hz
  DEFAULT_TIMEOUT = 500 # ms

  def initialize(unit:, frequency: DEFAULT_FREQUENCY, sda_pin: -1, scl_pin: -1, timeout: DEFAULT_TIMEOUT)
    @timeout = timeout
    @unit_num = _init(unit.to_s, frequency, sda_pin, scl_pin)
  end

  def scan(timeout: @timeout)
    msg_format = "I2C device found at 7-bit address 0x%02x (0b%07b) +%s"
    (0x08..0x77).each do |i2c_adrs_7|
      begin
        read(i2c_adrs_7, 1, timeout: timeout)
        puts sprintf(msg_format, i2c_adrs_7, i2c_adrs_7, "R")
      rescue IOError => e
        # p e
      end
      begin
        write(i2c_adrs_7, 0, timeout: timeout)
        puts sprintf(msg_format, i2c_adrs_7, i2c_adrs_7, "W")
      rescue IOError => e
        # p e
      end
    end
    nil
  end
end
