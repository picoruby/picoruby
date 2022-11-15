#! /usr/bin/env ruby

case RUBY_ENGINE
when "ruby"
  require_relative "../../picoruby-shell/mrblib/shell"
  require_relative "../../picoruby-vim/mrblib/vim"
when "mruby/c"
  require "shell"

  ENV['PATH'] = ["/bin"]
  ENV['HOME'] = "/home"
end

begin
  Shell.setup(:ram)
  IO.wait_and_clear
  Shell.new.start(:shell)
rescue => e
  puts e.message
  exit
end

