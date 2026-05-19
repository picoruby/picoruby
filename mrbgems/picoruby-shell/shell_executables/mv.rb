unless $*.size == 2
  puts "Usage: mv source_file target_file"
  return
end

source = $*[0]
target = $*[1]

unless File.exist?(source)
  puts "mv: cannot stat '#{source}': No such file or directory"
  return
end

begin
  File.rename(source, target)
rescue => e
  puts "mv: cannot move '#{source}' to '#{target}': #{e.message}"
end
