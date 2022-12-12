class Joystick
  # https://raspberrypi.github.io/pico-sdk-doxygen/group__hardware__adc.html
  # > input channel. 0...3 are GPIOs 26...29 respectively
  VALID_PINS = [26, 27, 28, 29]

  def initialize(axes = {})
    puts "Init Joystick"
    reset_axes
    axes.each do |axis, params|
      pin = params[:pin]
      unless pin.is_a? Integer
        puts "Invalid pin value: #{pin}"
        next
      end
      unless VALID_PINS.include?(pin)
        puts "Invaid joystick pin: #{pin}"
      else
        init_axis_offset(axis.to_s, pin - VALID_PINS[0])
        invert = params[:invert].nil? ? true : params[:invert]
        magnify = (params[:magnify] || 1).to_f
        unless magnify.is_a? Float
          puts "Invalid magnify value: #{magnify}"
          next
        end
        sens = magnify * (invert ? 1 : -1)
        # default value of sens: -1.0
        init_sensitivity(pin - VALID_PINS[0], sens)
      end
    end
  end
end
