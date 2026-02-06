if RUBY_ENGINE == "mruby/c"
  require "numeric-ext"
end
require "machine"
require "watchdog"
Watchdog.disable
require "shell"
require "irq"
STDOUT = IO.new
STDIN = IO.new

Machine.set_hwclock(0)

begin
  sleep 1
  STDIN.echo = false
  puts "Initializing FLASH disk as the root volume... "
  Shell.setup_root_volume(:flash, label: "R2P2")
  Shell.setup_system_files

  Shell.bootstrap("/etc/init.d/r2p2")

  shell = Shell.new(clean: true)
  puts "Starting shell...\n\n"

  shell.show_logo
  shell.start
rescue => e
  puts "#{e.message} (#{e.class})"
end

