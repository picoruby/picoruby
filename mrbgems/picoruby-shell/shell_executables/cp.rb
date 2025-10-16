# cp - copy files

unless ARGV.count == 2
  puts "Usage: cp source_file target_file"
  return
end

source = ARGV[0]
target = ARGV[1]

unless File.exist?(source)
  puts "cp: cannot stat '#{source}': No such file or directory"
  return
end

if File.directory?(source)
  puts "cp: omitting directory '#{source}'"
  return
end

begin
  File.open(source, 'r') do |src_file|
    File.open(target, 'w') do |dst_file|
      while chunk = src_file.read(512)
        dst_file.write(chunk)
      end
    end
  end
rescue => e
  puts "cp: cannot copy '#{source}' to '#{target}': #{e.message}"
end
