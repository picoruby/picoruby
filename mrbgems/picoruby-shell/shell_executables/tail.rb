# tail - output the last part of files

lines = 10

# Parse options
args = []
i = 0
while i < ARGV.size
  arg = ARGV[i]
  if arg == "-n" && i + 1 < ARGV.size
    lines = ARGV[i + 1].to_i
    i += 2
  elsif arg.start_with?("-")
    # Try to parse -N format
    num = arg[1..-1].to_i
    if num > 0
      lines = num
    else
      puts "tail: invalid option '#{arg}'"
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
    # Read from stdin - use circular buffer for memory efficiency
    buffer = []
    while line = gets
      buffer << line
      buffer.shift if buffer.size > lines
    end
    buffer.each { |line| puts line }
  else
    # Read from files
    args.each_with_index do |file, idx|
      unless File.exist?(file)
        puts "tail: cannot open '#{file}': No such file or directory"
        next
      end

      if args.size > 1
        puts "==> #{file} <==" if idx > 0 || idx == 0
        puts if idx > 0
      end

      # Use circular buffer to keep last N lines
      buffer = []
      File.open(file, 'r') do |f|
        while line = f.gets
          buffer << line
          buffer.shift if buffer.size > lines
        end
      end
      buffer.each { |line| puts line }
    end
  end
rescue => e
  puts "tail: #{e.message}"
end
