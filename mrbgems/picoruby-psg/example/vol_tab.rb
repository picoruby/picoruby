# vol_tab.rb

FULL_SCALE    = 12      # maximum value
HEADROOM_DB   = 3.0     # headroobm (dB)
STEP_DB       = 1.5     # decibel step (dB)
LEVELS        = 16      # PSG volume: 0..15

def db_to_amp(db)
  10.0 ** (db / 20.0)
end

headroom_gain = db_to_amp(-HEADROOM_DB)           # ~= 0.707
base          = ((1<<FULL_SCALE)-1) * headroom_gain        # 2895.‥

vol_tab = Array.new(LEVELS) do |i|
  if i.zero?
    0
  else
    rel_db = (i - (LEVELS - 1)) * STEP_DB         # eg: i=14 -> −1.5 dB
    (base * db_to_amp(rel_db)).round
  end
end

############################################################ output
puts "// volume table: #{STEP_DB} dB steps with #{HEADROOM_DB} dB headroom for #{FULL_SCALE}-bit range"
puts "static const uint16_t vol_tab[#{LEVELS}] = {"
puts "  #{vol_tab.join(', ')}"
puts "};"

############################################################ pan_tab
FULL = (1 << FULL_SCALE) - 1  # 4095 for 12-bit range
steps = 16
l_tab, r_tab = ["4095"], ["   0"]

0.upto(steps-2) do |i|
  theta = Math::PI/2 * i / (steps-2)
  l_tab << (FULL * Math.cos(theta)).round.to_s.rjust(4)
  r_tab << (FULL * Math.sin(theta)).round.to_s.rjust(4)
end

puts
puts "// Pan table: 1 = L-only,  8 = center, 15 = R-only. 0 is not supposed to be used."
puts "static const uint16_t pan_tab_l[#{steps}] = {"
puts "  " + l_tab.join(', ')
puts "};"
puts "static const uint16_t pan_tab_r[#{steps}] = {"
puts "  " + r_tab.join(', ')
puts "};"
