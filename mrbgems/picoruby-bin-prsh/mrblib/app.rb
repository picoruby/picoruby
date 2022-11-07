#! /usr/bin/env ruby

ENV = {}
ARGV = []

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

  IO.wait_and_clear
  File = MyFile
  Dir = MyDir
  FAT._setup(0) # Workaround until Flash ROM works
end

begin
  IO.wait_and_clear
  Shell.new.start(:prsh)
rescue
  exit
end

