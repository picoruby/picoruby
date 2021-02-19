ary = Array.new(3)
ary[0] = {a: 123e2}
ary[0][:key] = "string"
ary[1] = %w(abc ABC Hello)
ary[2] = 0x1f
ary[3] = !true
puts "my name is #{self.class}, result is #{ary[2] * ary[0][:a]}"
p ary
