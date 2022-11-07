arg = ARGV[0]

dir, file = if arg.nil?
  ["./", nil]
elsif arg == "/"
  ["/", nil]
elsif arg.end_with?("/")
  [arg, nil]
else
  elements = arg.split("/")
  file = elements.pop
  elements[0] = "/" if elements[0].to_s == ""
  [elements.join("/") , file]
end

if dir != "/" && Dir.exist?("#{dir}/#{file}")
  dir = "#{dir}/#{file}"
  file = nil
end

Dir.open(dir) do |dir|
  # TODO: handle file
  feed = false
  while entry = dir.read
    print entry, "  "
    feed = true
  end
  print "\n" if feed
end
