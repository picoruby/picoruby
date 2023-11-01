#
# 4-Wire RTD (PT100 Ohm)
#
# ┌┈┈┈┈┈┈┈┈┈┐  CRD
# ┊ ┌──══───────|<──── Iexc (GPIO)
# ┊ │ Rlead ┊
# ┊ ├──══───────────── AIN1 (ADC0) ┬
# ┊ │       ┊                      │
# ┊ ❚ Rrtd  ┊                      │ Vrtd
# ┊ │       ┊                      │
# ┊ ├──══───────────── AIN2 (ADC1) ┴
# ┊ │ Rlead ┊
# ┊ └──══───────┬───── REF+ (ADC2) ┬
# └┈┈┈┈┈┈┈┈┈┘   │                  │
#            ║  ❚ Rref             │ Vref
#               │                  │
#               └───── REF- (AGND) ┴
#

# usage:
#   rtd = RTD::PT100.new(
#     driver: MCP3424.new(...),
#     current_gpio: 18,
#     r_ref: 1000.0,
#     channels: {
#       rtd: 1,
#       ref: 2
#     }
#   )
# or
#   rtd = RTD::PT100.new(
#     driver: ADC,
#     current_gpio: 18,
#     r_ref: 1000.0,
#     channels: {
#       rtd: ADC.new(...),
#       ref: ADC.new(...)
#     }
#   ) 
class RTD
  DRIVERS = %i[ADC MCP3424 MCP3208]

  def initialize(driver:, current_gpio:, r_ref:, channels: {})
    if driver.to_s == "ADC"
      @driver = {ref: channels[:ref], rtd: channels[:rtd]}
      @driver_type = :ADC
    else
      @driver = driver
      @driver_type = driver.class.to_s.to_sym
    end
    DRIVERS.include?(@driver_type) or raise "Unknown driver: #{@driver.class}"
    @current_gpio = current_gpio
    @current_gpio.write 0
    @r_ref = r_ref.to_f
    @channels = channels
  end

  def read(unit: :celsius)
    @current_gpio.write 1
    driver = @driver
    channels = @channels
    case @driver_type
    when :ADC
      # @type var driver: Hash[Symbol, ADC]
      adc_rtd = driver[:rtd].read_raw
      adc_ref = driver[:ref].read_raw
    when :MCP3424
      # @type var driver: MCP3424
      # @type var channels: Hash[Symbol, Integer]
      adc_rtd = driver.one_shot_read(channels[:rtd])
      adc_ref = driver.one_shot_read(channels[:ref])
    when :MCP3208
      # TODO: implement
      adc_rtd = adc_ref = nil
    else
      adc_rtd = adc_ref = nil
    end
    @current_gpio.write 0
    if adc_rtd && adc_ref
      celcius = temperature_in_celsius(adc_rtd * @r_ref / adc_ref)
    else
      raise "driver read error"
    end
    case unit
    when :celsius
      celcius
    when :kelvin
      celcius + 273.15
    when :fahrenheit
      celcius * 9.0 / 5.0 + 32.0
    else
      raise "Unknown unit: #{unit}"
    end
  end

  def temperature_in_celsius(r)
    raise "Not implemented in RTD class. Use subclass."
  end

  class PT100 < RTD
    def temperature_in_celsius(r)
      8.2864E-09 * r**4 - 3.3720E-06 * r**3 + 1.5052E-03 * r**2 + 2.3255E+00 * r - 2.4506E+02
    end
  end
end

