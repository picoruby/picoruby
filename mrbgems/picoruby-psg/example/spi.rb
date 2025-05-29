
ldac=GPIO.new(16, GPIO::OUT)
ldac.write(0)
spi=SPI.new(unit: :RP2040_SPI0, copi_pin:19, sck_pin:18, cs_pin:17)

data = [0x3000|4000, 0x3000|3000, 0x3000|2000, 0x3000|1000, 0x3000]

while true
  data.each do |d|
    data1 = d>>8
    data2 = d&0xFF
    spi.select { |s| s.write([data1, data2]) }
    sleep 1
  end
end
