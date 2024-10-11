unless ARGV.count == 2
  puts "Usage: mv.rb source_file target_file"
  return
end

File.rename(ARGV[0], ARGV[1])
