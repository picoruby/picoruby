require "i2c"

# TODO: move /etc/init.c/r2p2-rtc
i2c_unit, sda, scl = :RP2040_I2C0, 4, 5

Shell.simple_question("Do you have RTC? (sda:#{sda}, scl:#{scl}) [y/N] ") do |answer|
  case answer
  when "y", "Y"
    i2c = I2C.new(unit: i2c_unit, sda_pin: sda, scl_pin: scl)
    rtc = PCF8523.new(i2c: i2c)
    Shell.setup_rtc(rtc)
    true
  when "n", "N", ""
    puts "No RTC"
    true
  end
end

