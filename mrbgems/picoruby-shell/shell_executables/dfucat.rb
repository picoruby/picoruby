path = ARGV[0]
unless path
  print "DFUCAT_ERROR:Usage: dfucat <path>\n"
  exit 1
end
begin
  data = File.open(path, "r") { |f| f.read }
  print "DFUCAT_BEGIN\n"
  print data
  print "DFUCAT_END\n"
rescue => e
  print "DFUCAT_ERROR:#{e.message}\n"
end
