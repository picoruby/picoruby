require "gpio"

class ADC
  def initialize(pin, additional_params = {})
    @additional_params = additional_params
    @min = nil
    @max = nil
    @input = ADC._init(pin)
    puts "@input: #{@input}"
    begin
      init_additional_params unless @additional_params.empty?
    rescue NoMethodError => e
      puts "You need to define `ADC#init_additional_params` if you use additional params"
      raise e
    end
  end
end

