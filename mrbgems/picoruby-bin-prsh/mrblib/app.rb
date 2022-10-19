require "vfs"
require "filesystem-fat"

File = MyFile
Dir = MyDir

fat = FAT.new("0")
VFS.mount(fat, "/")

p Dir.pwd
p ENV

p File.expand_path "..", "/home/matz/work/foo"
p File.expand_path "..", "/home"
p File.expand_path ".", "."

Dir.mkdir("test/")
Dir.mkdir("test/aa")
dir = Dir.new("/")
p Dir.empty?("/")
p Dir.empty?("/test")
p Dir.zero?("/test/aa")
dir.each do |f|
  puts f
end

file = File.open("test/myfile.txt", "w+")
file.puts "Hello!","World"
file.close
#file = File.open("test/myfile.txt", "r")
#lineno = 1
#file.each_line do |line|
#  #puts "lineno: #{lineno}"
#  p line
#  lineno += 1
#end
#file.close

File.open("test/myfile.txt") do |f|
  p :hey
  p f.gets
  p :hey
  p f.gets
end

