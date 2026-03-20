require "gpio"

# [Usage]
#
# Positive logic:
#   c_pin = GPIO.new(0, GPIO::OUT)
#   dip_switch = DipSwitch.new(c_pin, [1, 2, 3, 4])
#   dip_switch.read => 0..15
#
# Negative logic:
#   dip_switch = DipSwitch.new(nil, [1, 2, 3, 4])
#   dip_switch.read => 0..15
#
class DipSwitch
  def initialize(c_pin, pin_nums)
    @c_pin = c_pin
    @c_pin&.write 0
    @negative_logic = c_pin.nil?
    @pins = []
    pi = 0
    while pi < pin_nums.size
      pin = GPIO.new(pin_nums[pi], GPIO::IN)
      pin.setmode(@negative_logic ? GPIO::PULL_UP : GPIO::PULL_DOWN)
      @pins << pin
      pi += 1
    end
  end

  def read
    result = 0
    @c_pin&.write 1 unless @negative_logic
    pi = 0
    while pi < @pins.size
      result += (@pins[pi].read << pi)
      pi += 1
    end
    @c_pin&.write 0
    return result
  end
end

