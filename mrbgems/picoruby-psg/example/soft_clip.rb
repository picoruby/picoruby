def soft_clip(x)
  knee = 3600
  return x if x < knee
  d = x - knee
  y = knee + ((d>>3) - (d>>7))
  return y
end

0.step(9000, 500) do |x|
  puts "#{x} -> #{soft_clip(x)}"
end
