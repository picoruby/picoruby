unless ARGV.count == 2
  puts "Usage: mv source_file target_file"
  return
end

source = ARGV[0]
target = ARGV[1]

unless File.exist?(source)
  puts "mv: cannot stat '#{source}': No such file or directory"
  return
end

begin
  File.rename(source, target)
rescue => e
  puts "mv: cannot move '#{source}' to '#{target}': #{e.message}"
end
