unless ARGV.size == 2
  puts "Usage: mv source_file target_file"
  return
end

File.rename(ARGV[0], ARGV[1])
