B = 3435
To = 25
V = 3300 # mV
Rref = 10_000 # Ohm

class Thermistor
  def initialize
    gpio_init_output(0)
    gpio_set_level(0, 1)
    init_adc
  end

  def temperature
    vref = read_adc
    r = (V - vref).to_f / (vref.to_f/ Rref)
    1.to_f / ( 1.to_f / B * Math.log(r / Rref) + 1.to_f / (To + 273) ) - 273
  end
end
