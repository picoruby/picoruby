require "sandbox"
require "shell"
require "filesystem-fat"
require "vfs"
require "vim"

fat = FAT.new("0")
VFS.mount(fat, "/")
File = MyFile
Dir = MyDir

File.open("test.txt", "w") do |f|
  f.puts "hello"
  f.puts "world"
end

shell = Shell.new.start(:mrbsh)

