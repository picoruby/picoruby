path = ARGV[0]
unless path
  print "DFUCAT_ERROR:Usage: dfucat <path>\n"
  exit 1
end
begin
  puts "DFUCAT_BEGIN"
  File.open(path, "r") do |f|
    f.each_line do |line|
      print line
    end
  end
  puts "\nDFUCAT_END"
rescue => e
  puts "DFUCAT_ERROR:#{e.message}"
end
