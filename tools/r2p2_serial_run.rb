#!/usr/bin/env ruby

require "optparse"

RUN_DONE_MARKER = "__PICORUBY_RUN_DONE__"
DEFAULT_REMOTE_PATH = "/home/app.rb"
DEFAULT_BAUD = 115200
SHELL_PROMPT = "$> "
RECV_READY_MARKER = "READY"

options = {
  remote_path: DEFAULT_REMOTE_PATH,
  baud: DEFAULT_BAUD,
  command_delay_ms: 200,
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

def monotonic_now
  Process.clock_gettime(Process::CLOCK_MONOTONIC)
end

def format_bytes_per_sec(bytes_per_sec)
  return "0 B/s" if bytes_per_sec <= 0
  if bytes_per_sec >= 1024 * 1024
    format("%.2f MiB/s", bytes_per_sec / (1024.0 * 1024.0))
  elsif bytes_per_sec >= 1024
    format("%.2f KiB/s", bytes_per_sec / 1024.0)
  else
    format("%.0f B/s", bytes_per_sec)
  end
end

def format_duration(seconds)
  format("%.3fs", seconds)
end

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

def read_until_status(io, timeout_ms:, ok_markers:)
  buffer = +""
  deadline = Time.now + (timeout_ms / 1000.0)

  loop do
    remaining = deadline - Time.now
    raise "timeout waiting for response" if remaining <= 0

    ready = IO.select([io], nil, nil, remaining)
    raise "timeout waiting for response" unless ready

    chunk = io.read_nonblock(4096)
    if chunk
      buffer << chunk
      if buffer.include?("command not found")
        raise "shell command not found; did you flash the latest serial-runner firmware?"
      end
    end

    while (idx = buffer.index("\n"))
      line = buffer.slice!(0, idx + 1)
      yield line if block_given?
      stripped = line.strip
      return stripped if ok_markers.include?(stripped) || stripped.start_with?("ERROR:")
    end
  rescue IO::WaitReadable
    next
  end
end

def wait_for_prompt(io, timeout_ms:, prompt:)
  buffer = +""
  deadline = Time.now + (timeout_ms / 1000.0)

  loop do
    return if buffer.include?(prompt)

    remaining = deadline - Time.now
    raise "timeout waiting for shell prompt #{prompt.inspect}" if remaining <= 0

    ready = IO.select([io], nil, nil, remaining)
    raise "timeout waiting for shell prompt #{prompt.inspect}" unless ready

    chunk = io.read_nonblock(4096)
    next unless chunk

    buffer << chunk
  rescue IO::WaitReadable
    next
  end
end

def read_until_marker(io, marker, timeout_ms:, discard_until_newline: false)
  buffer = +""
  discard = discard_until_newline
  deadline = Time.now + (timeout_ms / 1000.0)

  loop do
    remaining = deadline - Time.now
    raise "timeout waiting for run output" if remaining <= 0

    ready = IO.select([io], nil, nil, remaining)
    raise "timeout waiting for run output" unless ready

    chunk = io.read_nonblock(4096)
    next unless chunk

    sanitized = chunk.gsub("\e", "").gsub(/[^\t\r\n[:print:]]/, "")
    if discard
      if (idx = sanitized.index("\n"))
        sanitized = sanitized[(idx + 1)..] || ""
        discard = false
      else
        sanitized = ""
      end
    end
    print sanitized unless sanitized.empty?
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
  io.write("\n")
  wait_for_prompt(io, timeout_ms: 3_000, prompt: SHELL_PROMPT)

  puts "Uploading #{local_path} -> #{options[:remote_path]} (#{payload.bytesize} bytes)"
  recv_command = "recv #{options[:remote_path]} #{payload.bytesize}\n"
  io.write(recv_command)
  recv_status = read_until_status(io, timeout_ms: 10_000, ok_markers: [RECV_READY_MARKER])
  unless recv_status == RECV_READY_MARKER
    warn recv_status
    exit 1
  end
  puts recv_status

  transfer_start = monotonic_now
  io.write(payload)

  recv_result = read_until_status(io, timeout_ms: 10_000, ok_markers: ["OK"])
  unless recv_result == "OK"
    warn recv_result
    exit 1
  end
  transfer_duration = monotonic_now - transfer_start
  puts recv_result
  puts "Transfer complete: #{payload.bytesize} bytes in #{format_duration(transfer_duration)} (#{format_bytes_per_sec(payload.bytesize / transfer_duration)})"

  run_command = "run #{options[:remote_path]}\n"
  puts "Running #{options[:remote_path]}"
  io.write(run_command)
  sleep(options[:command_delay_ms] / 1000.0)
  run_start = monotonic_now
  read_until_marker(io, RUN_DONE_MARKER, timeout_ms: 30_000, discard_until_newline: true)
  run_duration = monotonic_now - run_start
  puts "Run complete in #{format_duration(run_duration)}"
end
