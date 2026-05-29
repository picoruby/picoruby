begin
  require "vfs"
rescue LoadError
end

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
chunk_size = 512
chunk_timeout_ms = 15_000
littlefs_block_size = 4096

begin
  File.unlink(temp_path) if File.exist?(temp_path)
rescue => e
  puts "ERR #{e.message}"
  return
end

begin
  vfs = Object.const_get(:VFS)
  volume, = vfs.send(:sanitize_and_split, path)
  driver = volume && volume[:driver]
  if driver && driver.respond_to?(:sector_count)
    counts = driver.sector_count
    free_blocks = counts[:free] || 0
    free_bytes = free_blocks * littlefs_block_size
    if free_bytes < size
      puts "ERR no space: need=#{size} free=#{free_bytes}"
      return
    end
  end
rescue NameError
rescue => e
  puts "ERR #{e.message}"
  return
end

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
  GC.start
  STDIN.raw!
  puts "READY chunk=#{chunk_size}"

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
