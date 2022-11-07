ARGV.each do |arg|
  if File.exist?(arg)
    File.open arg, "r" do |f|
      f.each_line do |line|
        puts line
      end
    end
  else
    puts "cat: #{arg}: No such file"
  end
end
