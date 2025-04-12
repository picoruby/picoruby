ARGV.each do |arg|
  if File.file?(arg)
    File.open arg, "r" do |f|
      f.each_line do |line|
        print line
      end
    end
  else
    puts "cat: #{arg}: No such file"
  end
end
