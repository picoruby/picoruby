require "spi"

def setup_sd_card(spi_unit, sck, cipo, copi, cs)
  begin
    print "Initializing SD card... "
    spi = SPI.new(frequency: 5_000_000, unit: spi_unit,
                  sck_pin:  sck,  cipo_pin: cipo,
                  copi_pin: copi, cs_pin:   cs)
    sd = FAT.new(:sd, label: "SD", driver: spi)
    sd_mountpoint = "/sd"
    VFS.mount(sd, sd_mountpoint)
    puts "Available at #{sd_mountpoint}"
  rescue => e
    puts "Not available"
    puts "#{e.message} (#{e.class})"
  end
end

spi_unit, sck, cipo, copi, cs = :RP2040_SPI0, 22, 20, 23, 21
$shell.simple_question("Do you have SD card? (sck:#{sck}, cipo:#{cipo}, copi:#{copi}, cs:#{cs}) [y/N] ") do |answer|
  case answer
  when "y", "Y"
    setup_sd_card(spi_unit, sck, cipo, copi, cs)
    true
  when "n", "N", ""
    puts "No SD card"
    true
  end
end
