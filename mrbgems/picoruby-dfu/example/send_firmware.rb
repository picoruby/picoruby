#!/usr/bin/env ruby
#
# Send firmware to a R2P2 device running dfutcp.
#
# Usage:
#   ruby send_firmware.rb <firmware_file> [host] [port]
#
# Examples:
#   ruby send_firmware.rb app.rb
#   ruby send_firmware.rb app.rb 192.168.1.100
#   ruby send_firmware.rb app.mrb 192.168.1.100 4649
#

require 'socket'
require 'zlib'

firmware_path = ARGV[0]
host          = ARGV[1] || "localhost"
port          = (ARGV[2] || 4649).to_i

unless firmware_path
  $stderr.puts "Usage: ruby #{$0} <firmware_file> [host] [port]"
  exit 1
end

unless File.exist?(firmware_path)
  $stderr.puts "File not found: #{firmware_path}"
  exit 1
end

ext = File.extname(firmware_path).downcase
case ext
when ".mrb"
  type = "RITE"
when ".rb"
  type = "RUBY"
else
  $stderr.puts "Unsupported file extension: #{ext} (use .rb or .mrb)"
  exit 1
end

firmware = File.binread(firmware_path)
crc32    = Zlib.crc32(firmware)
sig_len  = 0

header = ["DFU\0", 1, type, firmware.size, crc32, sig_len].pack("a4Ca4NNn")

puts "Sending #{firmware_path} (#{type}, #{firmware.size} bytes, CRC32=0x#{crc32.to_s(16)})"
puts "Connecting to #{host}:#{port}..."

sock = TCPSocket.new(host, port)
sock.write(header)
sock.write(firmware)

response = sock.gets
sock.close

unless response
  $stderr.puts "No response from device"
  exit 1
end

puts response
exit 1 unless response.start_with?("OK")
