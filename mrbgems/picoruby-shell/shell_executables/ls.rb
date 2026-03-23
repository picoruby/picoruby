args = [] #: Array[String]
opts = [] #: Array[String]
ARGV.each do |arg|
  if arg.start_with?("-")
    arg[1, 255]&.each_char do |ch|
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
  elements[0] = "/" if elements == [""]
  [elements.join("/") , file]
end

if file
  _dir = "#{dir}/#{file}"
  if Dir.exist?(_dir) && File::Stat.new(_dir).directory?
    dir = _dir
    file = nil
  end
end

dir = "./" if dir.length == 0

Dir.open(dir) do |dirent|
  count = 0
  # TODO: Wildcard
  if opts.include?("l")
    label_printed = false
    while entry = dirent.read
      if !file || file == entry
        stat = File::Stat.new("#{dir}/#{entry}")
        if !label_printed
          begin
            puts "\e[36m#{Littlefs::Stat::LABEL}\e[0m"
          rescue NameError
          end
          label_printed = true
        end
        print "#{stat.size.to_s.rjust(8)}  #{stat.mtime.to_s}  #{entry}"
        stat.directory? ? puts("/") : puts
        count += 1
      end
    end
  else
    while entry = dirent.read
      if !file || file == entry
        puts entry
        count += 1
      end
    end
  end
  if file && count == 0
    puts "ls: cannot access '#{path}': No such file or directory"
  end
end
