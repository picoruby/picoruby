require "i2c"
require "adafruit_pcf8523"

def setup_rtc(i2c_unit, sda, scl)
  begin
    print "Initializing RTC... "
    ENV['TZ'] = "JST-9"
    i2c = I2C.new(unit: i2c_unit, sda_pin: sda, scl_pin: scl)
    $rtc = PCF8523.new(i2c: i2c)
    Time.hwclock = $rtc.current_time
    FAT.unixtime_offset = Time.unixtime_offset
    puts "Available (#{Time.now})"
  rescue => e
    puts "Not available"
    puts "#{e.message} (#{e.class})"
  end
end

i2c_unit, sda, scl = :RP2040_I2C0, 4, 5
$shell.simple_question("Do you have RTC? (sda:#{sda}, scl:#{scl}) [y/N] ") do |answer|
  case answer
  when "y", "Y"
    setup_rtc(i2c_unit, sda, scl)
    true
  when "n", "N", ""
    puts "No RTC"
    true
  end
end

