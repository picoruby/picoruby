require 'i2c'

class MCP3424
  DEFAULT_FREQUENCY = 100_000

  def initialize(unit:, frequency: DEFAULT_FREQUENCY, sda_pin: -1, scl_pin: -1, address_selector:)
    @i2c = I2C.new(unit: unit, frequency: frequency, sda_pin: sda_pin, scl_pin: scl_pin)
    set_address(address_selector)
    @configuration = 0
    # One-shot mode makes the device to enter power-down mode after a conversion.
    one_shot_read(0)
  end

  attr_reader :address

  def set_address(address_selector)
    if address_selector < 0 || 3 < address_selector
      raise ArgumentError, "address_selector must be 0..3"
    end
    @address = 0b0110_1000 | address_selector
  end

  def bit_resolution=(bit_resolution)
    @configuration &= 0b1111_0011
    case bit_resolution
    when 12
      # do nothing
    when 14
      @configuration |= 0b0000_0100
    when 16
      @configuration |= 0b0000_1000
    when 18
      @configuration |= 0b0000_1100
    else
      raise ArgumentError, "bit_resolution must be 12, 14, 16 or 18"
    end
  end

  def bit_resolution
    case @configuration & 0b0000_1100
    when 0b0000_0000
      12
    when 0b0000_0100
      14
    when 0b0000_1000
      16
    when 0b0000_1100
      18
    else
      raise RuntimeError, "bit_resolution is invalid"
    end
  end

  def pga_gain=(pga_gain)
    @configuration &= 0b1111_1100
    case pga_gain
    when 1
      # do nothing
    when 2
      @configuration |= 0b0000_0001
    when 4
      @configuration |= 0b0000_0010
    when 8
      @configuration |= 0b0000_0011
    else
      raise ArgumentError, "pga_gain must be 1, 2, 4 or 8"
    end
  end

  def pga_gain
    case @configuration & 0b0000_0011
    when 0b0000_0000
      1
    when 0b0000_0001
      2
    when 0b0000_0010
      4
    when 0b0000_0011
      8
    else
      raise RuntimeError, "pga_gain is invalid"
    end
  end

  def one_shot_read(channel, timeout_ms = 1000)
    assert_channel(channel)
    @i2c.write(@address, 0b1000_0000 | ((channel - 1) << 5) | @configuration)
    read(timeout_ms)
  end

  def start_continuous_conversion(channel)
    assert_channel(channel)
    @i2c.write(@address, 0b1001_0000 | ((channel - 1) << 5) | @configuration)
  end

  def read(timeout_ms = 1000)
    bit_resolution = self.bit_resolution
    read_length = (bit_resolution == 18) ? 4 : 3
    data = []
    while true
      d0, d1, d2, d3 = @i2c.read(@address, read_length).bytes
      return nil unless config_byte = (bit_resolution == 18) ? d3 : d2
      break if config_byte < 0b1000_0000 # equivalent to (config_byte & 0b1000_0000) == 0
      timeout_ms -= 10
      return nil if timeout_ms < 0
      sleep_ms 10
    end
    case bit_resolution
    when 12
      ((d0 || 0) << 8) | (d1 || 0)
    when 14
      (((d0 || 0) & 0b0011_1111) << 8) | (d1 || 0)
    when 16
      ((d0 || 0) << 8) | (d1 || 0)
    when 18
      (((d0 || 0) & 0b0000_0011) << 16) | ((d1 || 0) << 8) | (d2 || 0)
    end
  end

  # private

  def assert_channel(channel)
    if channel < 1 || 4 < channel
      raise ArgumentError, "channel must be 1..4"
    end
  end
end

