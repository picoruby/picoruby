unless ARGV.size == 1
  puts "Usage: rapicco <path_to_rapicco_file>"
  return
end

begin
  require 'rapicco'
rescue
  # maybe MicroRuby
end

Machine.signal_self_manage
Rapicco.new(ARGV[0]).run
