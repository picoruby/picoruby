# head - output the first part of files

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
    args.each_with_index do |file, idx|
      unless File.exist?(file)
        puts "head: cannot open '#{file}': No such file or directory"
        next
      end

      if 1 < args.size
        puts "==> #{file} <==" if idx > 0 || idx == 0
        puts if 0 < idx
      end

      File.open(file, 'r') do |f|
        count = 0
        while count < lines && (line = f.gets)
          puts line
          count += 1
        end
      end
    end
  end
rescue => e
  puts "head: #{e.message}"
end
