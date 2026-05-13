#!/usr/bin/env ruby

require "optparse"

RUN_DONE_MARKER = "__PICORUBY_RUN_DONE__"
DEFAULT_REMOTE_PATH = "/home/app.rb"
DEFAULT_BAUD = 115200
SHELL_PROMPT = "$> "

options = {
  remote_path: DEFAULT_REMOTE_PATH,
  baud: DEFAULT_BAUD,
  command_delay_ms: 200,
  chunk_timeout_ms: 5_000,
  skip_run: false,
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

  opt.on("--chunk-timeout-ms MS", Integer, "Per-chunk ACK timeout in milliseconds (default: #{options[:chunk_timeout_ms]})") do |value|
    options[:chunk_timeout_ms] = value
  end

  opt.on("--skip-run", "Upload only; do not run the remote file") do
    options[:skip_run] = true
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

if options[:chunk_timeout_ms] <= 0
  warn "--chunk-timeout-ms must be positive"
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

def sanitize_serial_text(text)
  text.gsub("\e", "").gsub(/[^\t\r\n[:print:]]/, "")
end

def read_transfer_status(io, timeout_ms:)
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
    end

    while (idx = buffer.index("\n"))
      line = buffer.slice!(0, idx + 1)
      sanitized = sanitize_serial_text(line).strip
      next if sanitized.empty?
      if sanitized.include?("command not found")
        raise "shell command not found; did you flash the latest serial-runner firmware?"
      end
      return sanitized if sanitized.start_with?("READY", "ACK ", "OK", "ERR ")
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

    sanitized = sanitize_serial_text(chunk)
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

def print_transfer_progress(acked_bytes, total_bytes, started_at)
  elapsed = monotonic_now - started_at
  percent = total_bytes.zero? ? 100.0 : (acked_bytes * 100.0 / total_bytes)
  speed = elapsed > 0 ? acked_bytes / elapsed : 0
  print "\rProgress: #{acked_bytes}/#{total_bytes} bytes (#{format("%.1f", percent)}%) ack=#{acked_bytes} elapsed=#{format_duration(elapsed)} speed=#{format_bytes_per_sec(speed)}"
  $stdout.flush
end

def send_chunked_payload(io, payload, chunk_size, chunk_timeout_ms)
  offset = 0
  transfer_start = monotonic_now
  last_ack_offset = 0

  while offset < payload.bytesize
    chunk = payload.byteslice(offset, [payload.bytesize - offset, chunk_size].min)
    written = io.write(chunk)
    raise "short serial write" unless written == chunk.bytesize

    begin
      status = read_transfer_status(io, timeout_ms: chunk_timeout_ms)
    rescue => e
      raise "#{e.message} (last ACK #{last_ack_offset}/#{payload.bytesize} bytes)"
    end
    if status.start_with?("ERR ")
      raise "device error after ACK #{last_ack_offset}: #{status.sub(/\AERR\s+/, "")}"
    end

    unless status =~ /\AACK (\d+)\z/
      raise "unexpected transfer status after sending chunk: #{status.inspect}"
    end

    ack_offset = Regexp.last_match(1).to_i
    expected_offset = offset + chunk.bytesize
    unless ack_offset == expected_offset
      raise "unexpected ACK offset #{ack_offset} (expected #{expected_offset})"
    end

    last_ack_offset = ack_offset
    offset = ack_offset
    print_transfer_progress(last_ack_offset, payload.bytesize, transfer_start)
  end

  begin
    final_status = read_transfer_status(io, timeout_ms: chunk_timeout_ms)
  rescue => e
    raise "#{e.message} (last ACK #{last_ack_offset}/#{payload.bytesize} bytes)"
  end
  puts

  if final_status.start_with?("ERR ")
    raise "device error after ACK #{last_ack_offset}: #{final_status.sub(/\AERR\s+/, "")}"
  end

  unless final_status =~ /\AOK (\d+)\z/
    raise "unexpected final transfer status: #{final_status.inspect}"
  end

  committed_size = Regexp.last_match(1).to_i
  unless committed_size == payload.bytesize
    raise "device committed unexpected size #{committed_size} (expected #{payload.bytesize})"
  end

  {
    duration: monotonic_now - transfer_start,
    ack_offset: last_ack_offset,
    committed_size: committed_size,
  }
rescue => e
  puts
  raise "#{e.message}"
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
  recv_status = read_transfer_status(io, timeout_ms: 10_000)
  unless recv_status =~ /\AREADY chunk=(\d+)\z/
    warn recv_status
    exit 1
  end
  negotiated_chunk_size = Regexp.last_match(1).to_i
  puts recv_status

  transfer_result = send_chunked_payload(io, payload, negotiated_chunk_size, options[:chunk_timeout_ms])
  puts "Transfer complete: #{payload.bytesize} bytes in #{format_duration(transfer_result[:duration])} (#{format_bytes_per_sec(payload.bytesize / transfer_result[:duration])})"

  next if options[:skip_run]

  run_command = "run #{options[:remote_path]}\n"
  puts "Running #{options[:remote_path]}"
  io.write(run_command)
  sleep(options[:command_delay_ms] / 1000.0)
  run_start = monotonic_now
  read_until_marker(io, RUN_DONE_MARKER, timeout_ms: 30_000, discard_until_newline: true)
  run_duration = monotonic_now - run_start
  puts "Run complete in #{format_duration(run_duration)}"
end
