#driver = PSG::Driver.new(:pwm, left: 26, right: 27)

driver = PSG::Driver.new(:mcp4921, copi: 19, sck: 18, cs: 17, ldac: 16)
p driver.send_reg(7, 0b111000) # Enable tone A, B, C
vol = 1
p driver.send_reg(8, vol)
p driver.send_reg(9, vol)
p driver.send_reg(10, vol)

# Clock: 2MHz
# period = 2_000_000 / (32 * f)
notes = {
  C4: 2_000_000 / (32 * 261),
  D4: 2_000_000 / (32 * 294),
  E4: 2_000_000 / (32 * 329),
  F4: 2_000_000 / (32 * 349),
  G4: 2_000_000 / (32 * 392),
  A4: 2_000_000 / (32 * 440),
  B4: 2_000_000 / (32 * 494),
  C5: 2_000_000 / (32 * 523),
}

c4 = notes[:C4]
e4 = notes[:E4]
g4 = notes[:G4]
4.times do |i|
  driver.set_timbre(0, i)
  p driver.send_reg(0, c4 & 0xFF)
  p driver.send_reg(1, (c4 >> 8) & 0x0F)
  sleep_ms 500
  p driver.send_reg(0, g4 & 0xFF)
  p driver.send_reg(1, (g4 >> 8) & 0x0F)
  sleep_ms 500
end
#p driver.send_reg(2, e4 & 0xFF)
#p driver.send_reg(3, (e4 >> 8) & 0x0F)
#sleep_ms 1000
#p driver.send_reg(4, g4 & 0xFF)
#p driver.send_reg(5, (g4 >> 8) & 0x0F)
#sleep_ms 1000

# Stop the tone
p driver.send_reg(8, 0)
p driver.send_reg(9, 0)
p driver.send_reg(10, 0)

driver.stop
