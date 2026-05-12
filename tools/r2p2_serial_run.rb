#!/usr/bin/env ruby

require "optparse"

RUN_DONE_MARKER = "__PICORUBY_RUN_DONE__"
DEFAULT_REMOTE_PATH = "/home/app.rb"
DEFAULT_BAUD = 115200

options = {
  remote_path: DEFAULT_REMOTE_PATH,
  baud: DEFAULT_BAUD,
  command_delay_ms: 50,
}

parser = OptionParser.new do |opt|
  opt.banner = "Usage: ruby tools/r2p2_serial_run.rb <serial_port> <local_file> [options]"

  opt.on("--remote-path PATH", "Destination path on Pico (default: #{DEFAULT_REMOTE_PATH})") do |value|
    options[:remote_path] = value
  end

  opt.on("--baud RATE", Integer, "Baud rate (default: #{DEFAULT_BAUD})") do |value|
    options[:baud] = value
  end

  opt.on("--command-delay-ms MS", Integer, "Delay after each shell command before sending payload") do |value|
    options[:command_delay_ms] = value
  end
end

parser.parse!(ARGV)

serial_port = ARGV[0]
local_path = ARGV[1]

unless serial_port && local_path
  warn parser.banner
  exit 1
end

unless File.file?(local_path)
  warn "File not found: #{local_path}"
  exit 1
end

payload = File.binread(local_path)

def configure_serial!(path, baud)
  port_flag = RUBY_PLATFORM.include?("darwin") ? "-f" : "-F"
  command = [
    "stty", port_flag, path,
    baud.to_s,
    "raw",
    "-echo",
    "-icanon",
    "min", "0",
    "time", "0"
  ]
  success = system(*command)
  raise "failed to configure serial device: #{path}" unless success
end

def drain_input(io)
  loop do
    chunk = io.read_nonblock(4096)
    break if chunk.nil? || chunk.empty?
  rescue IO::WaitReadable, EOFError
    break
  end
end

def read_until_line(io, timeout_ms:)
  buffer = +""
  deadline = Time.now + (timeout_ms / 1000.0)

  loop do
    remaining = deadline - Time.now
    raise "timeout waiting for response" if remaining <= 0

    ready = IO.select([io], nil, nil, remaining)
    raise "timeout waiting for response" unless ready

    chunk = io.read_nonblock(4096)
    buffer << chunk if chunk

    while (idx = buffer.index("\n"))
      line = buffer.slice!(0, idx + 1)
      yield line if block_given?
      stripped = line.strip
      return stripped if stripped == "OK" || stripped.start_with?("ERROR:")
    end
  rescue IO::WaitReadable
    next
  end
end

def read_until_marker(io, marker, timeout_ms:)
  buffer = +""
  deadline = Time.now + (timeout_ms / 1000.0)

  loop do
    remaining = deadline - Time.now
    raise "timeout waiting for run output" if remaining <= 0

    ready = IO.select([io], nil, nil, remaining)
    raise "timeout waiting for run output" unless ready

    chunk = io.read_nonblock(4096)
    next unless chunk

    print chunk
    buffer << chunk
    return if buffer.include?(marker)
  rescue IO::WaitReadable
    next
  end
end

configure_serial!(serial_port, options[:baud])

File.open(serial_port, "r+") do |io|
  io.sync = true
  drain_input(io)

  recv_command = "recv #{options[:remote_path]} #{payload.bytesize}\n"
  io.write(recv_command)
  sleep(options[:command_delay_ms] / 1000.0)
  io.write(payload)

  recv_result = read_until_line(io, timeout_ms: 10_000)
  unless recv_result == "OK"
    warn recv_result
    exit 1
  end

  run_command = "run #{options[:remote_path]}\n"
  io.write(run_command)
  read_until_marker(io, RUN_DONE_MARKER, timeout_ms: 30_000)
end
