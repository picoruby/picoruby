class SDMMC
  # SDMMC bus width modes
  WIDTH_1BIT = 1
  WIDTH_4BIT = 4

  # Pin accessors are defined in C (src/mrubyc/sdmmc.c)
  # clk_pin, cmd_pin, d0_pin, d1_pin, d2_pin, d3_pin, width

  def self.new(clk_pin:, cmd_pin:, d0_pin:, d1_pin: -1, d2_pin: -1, d3_pin: -1, width: WIDTH_1BIT)
    sdmmc = self._init(clk_pin, cmd_pin, d0_pin, d1_pin, d2_pin, d3_pin, width)
    sdmmc
  end
end
