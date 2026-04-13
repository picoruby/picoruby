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
display.draw_text("shinonome_min16", 28, 20, "Lチカは情操教育", 2)
display.draw_text("terminus_12x24", 28, 70, "@hasumikin", 2)
display.update
