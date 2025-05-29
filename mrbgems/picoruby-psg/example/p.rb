#driver = PSG::Driver.new(:pwm, left: 2, right: 3)
driver = PSG::Driver.new(:mcp4921, copi: 19, sck: 18, cs: 17, ldac: 16)

# Enable tone on A, B, C. Disable noise.
driver.send_reg(7, 0b00111000) # Bit 0–2 = 0 (enable tone), 3–5 = 1 (disable noise)

# Note periods (assume 3.579545 MHz master clock, divide by 16)
NOTES = {
  C4: 856, D4: 762, E4: 678, F4: 640,
  G4: 570, A4: 512, B4: 456, C5: 428
}

# Define a short melody using chords
# Each chord: [noteA, noteB, noteC]
song = [
  [:C4, :E4, :G4],  # C major
  [:D4, :F4, :A4],  # D minor
  [:E4, :G4, :B4],  # E minor
  [:F4, :A4, :C5],  # F major
  [:G4, :B4, :D4],  # G major
  [:C4, :E4, :G4],  # Back to C
]

# Play the song
song.each do |a, b, c|
  driver.send_reg(0, NOTES[a] & 0xFF)
  driver.send_reg(1, (NOTES[a] >> 8) & 0x0F)

#  driver.send_reg(2, NOTES[b] & 0xFF)
#  driver.send_reg(3, (NOTES[b] >> 8) & 0x0F)
#
#  driver.send_reg(4, NOTES[c] & 0xFF)
#  driver.send_reg(5, (NOTES[c] >> 8) & 0x0F)

  # Set volumes (simple fixed volume)
  driver.send_reg(8, 0x0A)
#  driver.send_reg(9, 0x0A)
#  driver.send_reg(10, 0x0A)

  sleep_ms 1000
end

# Stop all tones
driver.send_reg(8, 0)
driver.send_reg(9, 0)
driver.send_reg(10, 0)
driver.stop
