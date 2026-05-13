unless ARGV.size == 2
  puts "ERROR: Usage: recv <path> <size>"
  return
end

path = ARGV[0]
size_arg = ARGV[1]
size = size_arg.to_i

unless size_arg == size.to_s
  puts "ERROR: size must be an integer"
  return
end

if size < 0
  puts "ERROR: size must be non-negative"
  return
end

Machine.signal_self_manage

temp_path = "#{path}.part"
received = 0
waited_ms = 0
timeout_ms = 5000

begin
  STDIN.raw!
  puts "READY"

  File.unlink(temp_path) if File.exist?(temp_path)
  File.open(temp_path, "w") do |file|
    while received < size
      chunk = STDIN.read_nonblock([size - received, 256].min)
      if chunk
        file.write(chunk)
        received += chunk.bytesize
        waited_ms = 0
      else
        if timeout_ms <= waited_ms
          raise "timeout while receiving data"
        end
        sleep_ms 1
        waited_ms += 1
      end
    end
  end

  File.unlink(path) if File.exist?(path)
  File.rename(temp_path, path)
  puts "OK"
rescue => e
  File.unlink(temp_path) if File.exist?(temp_path)
  puts "ERROR: #{e.message}"
ensure
  STDIN.cooked!
end
