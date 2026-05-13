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
chunk_size = 256
chunk_timeout_ms = 5000

read_exact = ->(expected_size) do
  buffer = +""
  waited_ms = 0

  while buffer.bytesize < expected_size
    chunk = STDIN.read_nonblock(expected_size - buffer.bytesize)
    if chunk && !chunk.empty?
      buffer << chunk
      waited_ms = 0
    else
      if chunk_timeout_ms <= waited_ms
        raise "timeout while receiving data (#{received}/#{size} bytes)"
      end
      sleep_ms 1
      waited_ms += 1
    end
  end

  buffer
end

begin
  STDIN.raw!
  puts "READY chunk=#{chunk_size}"

  File.unlink(temp_path) if File.exist?(temp_path)
  File.open(temp_path, "w") do |file|
    while received < size
      expected_size = [size - received, chunk_size].min
      chunk = read_exact.call(expected_size)
      file.write(chunk)
      received += chunk.bytesize
      puts "ACK #{received}"
    end
  end

  File.unlink(path) if File.exist?(path)
  File.rename(temp_path, path)
  puts "OK #{size}"
rescue => e
  File.unlink(temp_path) if File.exist?(temp_path)
  puts "ERR #{e.message}"
ensure
  STDIN.cooked!
end
