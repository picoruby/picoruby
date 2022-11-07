args = []
opts = []
ARGV.each do |arg|
  if arg.start_with?("-")
    arg[1, 255].each_char do |ch|
      opts << ch
    end
  else
    args << arg
  end
end

path = args[0]

dir, file = if path.nil?
  ["./", nil]
elsif path == "/"
  ["/", nil]
elsif path.end_with?("/")
  [path, nil]
else
  elements = path.split("/")
  file = elements.pop
  elements[0] = "/" if elements[0].to_s == ""
  [elements.join("/") , file]
end

if dir != "/" && Dir.exist?("#{dir}/#{file}")

Dir.open(dir) do |d|
  # TODO: handle file
  if opts.include?("l")
    label = d.stat_label
    puts "\e[36m#{label[:attributes]} #{label[:size].to_s.rjust(6)} #{label[:datetime]}\e[0m"
    while entry = d.read
      stat = File.stat("#{dir}/#{entry}")
      puts "#{stat[:attributes]} #{stat[:size].to_s.rjust(6)} #{stat[:datetime]} #{entry}"
    end
  else
    feed = false
    while entry = d.read
      print entry, "  "
      feed = true
    end
    puts if feed
  end
end
