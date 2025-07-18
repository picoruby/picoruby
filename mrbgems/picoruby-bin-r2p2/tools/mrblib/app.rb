#! /usr/bin/env ruby

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
  puts e.message
  Machine.exit(1)
end
