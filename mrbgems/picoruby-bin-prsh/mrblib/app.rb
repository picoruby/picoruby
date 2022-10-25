#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby"
  require_relative "../../picoruby-shell/mrblib/shell"
  require_relative "../../picoruby-vim/mrblib/vim"
when "mruby/c"
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
end

shell = Shell.new.start(:mrbsh)

