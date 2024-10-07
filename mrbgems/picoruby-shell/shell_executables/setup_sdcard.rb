require "spi"

# TODO: move /etc/init.c/r2p2-sdcard
spi_unit, sck, cipo, copi, cs = :RP2040_SPI0, 22, 20, 23, 21

$shell.simple_question("Do you have SD card? (sck:#{sck}, cipo:#{cipo}, copi:#{copi}, cs:#{cs}) [y/N] ") do |answer|
  case answer
  when "y", "Y"
    spi = SPI.new(frequency: 5_000_000, unit: spi_unit,
                    sck_pin:  sck,  cipo_pin: cipo,
                    copi_pin: copi, cs_pin:   cs)
    Shell.setup_sdcard(spi)
    true
  when "n", "N", ""
    puts "No SD card"
    true
  end
end
