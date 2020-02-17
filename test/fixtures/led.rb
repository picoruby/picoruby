class Led
  def initialize(pin)
    @pin = pin
    gpio_init_output(@pin)
    turn_off
    @value = 1
  end

  def turn_on
    return nil if @value == 1
    gpio_set_level(@pin, 1)
    @value = 1
#    puts "turned on"
  end

  def turn_off
    return nil if @value == 0
    gpio_set_level(@pin, 0)
    @value = 0
#    puts "turned off"
  end
end
