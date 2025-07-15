unless ARGV.size == 1
  puts "Usage: rapicco <path_to_rapicco_file>"
  return
end

ENV['SIGNAL_SELF_MANAGE'] = 'yes'
Rapicco.new(ARGV[0]).run
