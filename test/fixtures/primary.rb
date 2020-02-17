debugprint('start', 'main_loop')

$co2 = Co2.new
$thermistor = Thermistor.new
#$wifi = Wifi.new

led = Led.new(19)

while true
  co2 = $co2.concentrate
  temperature = $thermistor.temperature
  puts "CO2: #{co2}, Temperature: #{temperature}"
  if co2 > 2000
    20.times do
      led.turn_on
      sleep 0.1
      led.turn_off
      sleep 0.1
    end
  elsif co2 > 1500
    led.turn_off
    sleep 4.2
    led.turn_on
    4.times do
      led.turn_off
      sleep 0.1
      led.turn_on
      sleep 0.1
    end
  elsif co2 > 1000
    led.turn_off
    sleep 4.6
    2.times do
      led.turn_off
      sleep 0.1
      led.turn_on
      sleep 0.1
    end
  else
    led.turn_off
    sleep 4.9
    led.turn_on
    sleep 0.1
  end
end
