#! /usr/bin/env ruby

require "numeric-ext"
require "shell"

begin
  IO.wait_terminal and IO.clear_screen
  Shell.setup_system_files("#{Dir.pwd}/.r2p2")

  Shell.bootstrap("../etc/init.d/r2p2")

  shell = Shell.new
  shell.show_logo
  shell.start
rescue Interrupt
  puts "\nExiting as Interrupted"
  Machine.exit(0)
rescue => e
  puts "#{e.message} (#{e.class})"
  puts e.backtrace if e.respond_to?(:backtrace)
  Machine.exit(1)
end
