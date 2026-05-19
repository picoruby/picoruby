unless $*.size == 1
  puts "Usage: rapicco <path_to_rapicco_file>"
  return
end

begin
  require 'rapicco'
rescue
  # maybe PicoRuby
end

Machine.signal_self_manage
Rapicco.new($*[0]).run
