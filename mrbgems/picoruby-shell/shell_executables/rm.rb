ARGV.each do |arg|
  if File.exist?(arg)
    File.unlink(arg)
  else
    puts "rm: cannot remove '#{arg}': No such file"
  end
end
