#! /usr/bin/env ruby

require "shell"

begin
  IO.wait_terminal and IO.clear_screen
  Shell.setup_system_files("#{Dir.pwd}/.r2p2", force: true)

  Shell.bootstrap("../etc/init.d/r2p2")

  $shell = Shell.new
  $shell.show_logo
  $shell.start
rescue => e
  puts e.message
  exit
end
