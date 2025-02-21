#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby"
  require_relative "../../picoruby-shell/mrblib/shell"
  require_relative "../../picoruby-vim/mrblib/vim"
when "mruby/c"
  require "dir"
  require "shell"
end

begin
  IO.wait_terminal and IO.clear_screen
  Shell.setup_system_files("#{Dir.pwd}/.r2p2", force: true)
  $shell = Shell.new
  $shell.show_logo
  $shell.start
rescue => e
  puts e.message
  exit
end

