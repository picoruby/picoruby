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
    pin_nums.each do |pin_num|
      pin = GPIO.new(pin_num, GPIO::IN)
      pin.setmode(@negative_logic ? GPIO::PULL_UP : GPIO::PULL_DOWN)
      @pins << pin
    end
  end

  def read
    result = 0
    @c_pin&.write 1 unless @negative_logic
    @pins.each_with_index do |pin, index|
      result += (pin.read << index)
    end
    @c_pin&.write 0
    return result
  end
end

