if !ARGV[1]
  puts "No output file specified"
elsif File.exist?(ARGV[0])
  script = ""
  File.open(ARGV[0], "r") do |f|
    script = f.read(f.size) # TODO: check size
  end
  begin
    mrb = self.mrbc(script)
    File.open(ARGV[1], "w") do |f|
      # f.expand(mrb.length)
      f.write(mrb)
    end
  rescue => e
    puts "#{e.message}"
  end
else
  puts "#{ARGV[0]}: No such file"
end
