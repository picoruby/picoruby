require "uc8151"

spi = SPI.new(
  unit: :RP2040_SPI0,
  frequency: 2_000_000,
  sck_pin:  18,
  copi_pin: 19
)
display = UC8151.new(
  spi:      spi,
  cs_pin:   17,
  dc_pin:   20,
  rst_pin:  21,
  busy_pin: 26
)
display.fill(0)
display.draw_text("karmatic-arcade_10", 10,  20, "hasumikin", 3)
display.draw_text("karmatic-arcade_10", 10,  55, "hasumi hitoshi")
display.draw_line(0, 70, 296, 70)
display.draw_text("terminus_12x24",     10,  75, "Creator of PicoRuby")
display.draw_text("shinonome_go16",     10, 105, "座右の銘: Lチカは情操教育")
display.update
