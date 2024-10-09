class ADC
  def initialize(pin, additional_params = {})
    @additional_params = additional_params
    @input = _init(pin)
    begin
      init_additional_params unless @additional_params.empty?
    rescue NoMethodError => e
      puts "You need to define `ADC#init_additional_params` if you use additional params"
      raise e
    end
  end
end

