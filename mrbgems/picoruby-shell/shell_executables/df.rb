# df - report file system disk space usage

begin
  require 'vfs'
rescue LoadError
  puts "VFS not available"
  return
end

puts "Filesystem           Mounted on"

if VFS::VOLUMES.empty?
  puts "No filesystems mounted"
else
  VFS::VOLUMES.each do |volume|
    mountpoint = volume[:mountpoint]
    driver_class = volume[:driver].class.to_s
    # Pad filesystem name to 20 characters
    fs_name = driver_class.ljust(20)
    puts "#{fs_name} #{mountpoint}"
  end
end
