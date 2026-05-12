RUN_DONE_MARKER = "__PICORUBY_RUN_DONE__"

unless ARGV.size == 1
  puts "ERROR: Usage: run <path>"
  puts RUN_DONE_MARKER
  return
end

path = ARGV[0]

unless File.file?(path)
  puts "ERROR: #{path}: No such file"
  puts RUN_DONE_MARKER
  return
end

Machine.signal_self_manage

begin
  load path
rescue Exception => e
  puts "#{e.message} (#{e.class})"
ensure
  puts RUN_DONE_MARKER
end
