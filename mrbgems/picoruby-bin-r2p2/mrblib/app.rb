#! /usr/bin/env ruby


case RUBY_ENGINE
when "ruby"
  require_relative "../../picoruby-shell/mrblib/shell"
  require_relative "../../picoruby-vim/mrblib/vim"
when "mruby/c"
  require "shell"

  ENV['TZ'] = "JST-9"
  ENV['PATH'] = "/bin"
  ENV['HOME'] = "/home"
end

begin
  IO.wait_terminal and IO.clear_screen
  $shell = Shell.new
  $shell.show_logo
  $shell.setup_root_volume(:ram)
  $shell.setup_system_files
  $shell.start
rescue => e
  puts e.message
  exit
end

