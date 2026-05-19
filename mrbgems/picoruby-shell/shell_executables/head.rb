# head - output the first part of files

lines = 10

# Parse options
args = [] #: Array[String]
i = 0
while i < $*.size
  arg = $*[i]
  if arg == "-n" && i + 1 < $*.size
    lines = $*[i + 1].to_i
    i += 2
  elsif arg.start_with?("-")
    # Try to parse -N format
    num = arg[1..-1].to_i
    if num > 0
      lines = num
    else
      puts "head: invalid option '#{arg}'"
      return
    end
    i += 1
  else
    args << arg
    i += 1
  end
end

begin
  if args.empty?
    # Read from stdin
    count = 0
    while count < lines && (line = gets)
      puts line
      count += 1
    end
  else
    # Read from files
    idx = 0
    while idx < args.size
      file = args[idx]
      unless File.exist?(file)
        puts "head: cannot open '#{file}': No such file or directory"
        idx += 1
        next
      end

      if 1 < args.size
        puts if 0 < idx
        puts "==> #{file} <=="
      end

      File.open(file, 'r') do |f|
        count = 0
        while count < lines && (line = f.gets)
          puts line
          count += 1
        end
      end
      idx += 1
    end
  end
rescue => e
  puts "head: #{e.message}"
end
