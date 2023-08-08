require "adc"

#
# 4-Wire RTD (PT100 Ohm)
#
# ┌┈┈┈┈┈┈┈┈┈┐  CRD
# ┊ ┌──══───────|<──── Iexc (GPIO)
# ┊ │ Rlead ┊
# ┊ ├──══───────────── AIN1 (ADC0) ┬
# ┊ │       ┊                      │
# ┊ ║ Rrtd  ┊                      │ Vrtd
# ┊ │       ┊                      │
# ┊ ├──══───────────── AIN2 (ADC1) ┴
# ┊ │ Rlead ┊
# ┊ └──══───────┬───── REF+ (ADC2) ┬
# └┈┈┈┈┈┈┈┈┈┘   │                  │
#               ║ Rref             │ Vref
#               │                  │
#               └───── REF- (AGND) ┴
#

class RTD
  def initialize(iexc_pin, ain1_pin, ain2_pin, ref_pin, r_ref)
    @iexc = GPIO.new(iexc_pin, GPIO::OUT)
    @iexc.write 0
    @iexc.write 1
    @ain1 = ADC.new(ain1_pin)
    @ain2 = ADC.new(ain2_pin)
    @ref = ADC.new(ref_pin)
    @r_ref = r_ref
    @sample_count = 1
  end

  attr_accessor :sample_count

  def read
    value = 0.0
    @iexc.write 1
    sleep 0.01
    @sample_count.times do
      ain1 = @ain1.read
      ain2 = @ain2.read
      ref = @ref.read
      value += (ain1 - ain2) / ref.to_f * @r_ref
    end
    @iexc.write 0
    value / @sample_count
  end
end

