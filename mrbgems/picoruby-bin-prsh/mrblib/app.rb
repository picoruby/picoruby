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

  fat = FAT.new("ram")
  retry_count = 0
  begin
    VFS.mount(fat, "/")
  rescue => e
    if retry_count < 1 &&
        e.message.include?("Storage device not ready") &&
        (fat.mkfs == 0)
      retry_count += 1
      retry
    else
      raise e
    end
  end
  File = MyFile
  Dir = MyDir

  File.open("test.txt", "w") do |f|
    f.puts "hello"
    f.puts "world"
  end
end

begin
  IO.wait_and_clear
  Shell.new.start(:prsh)
rescue
  exit
end

