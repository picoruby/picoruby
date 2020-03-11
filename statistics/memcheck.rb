allocs = Array.new
frees  = Array.new

File.open(ARGV[0]) do |f|
  f.each_line do |line|
    if data = line.scrub .match(/\A\s+alloc: (\w+)/)
      allocs << data[1]
    elsif data = line.scrub .match(/\A\s+free: (\w+)/)
      frees << data[1]
    end
  end
end

puts allocs.uniq.sort - frees.uniq.sort
