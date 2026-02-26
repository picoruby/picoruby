#!/usr/bin/env ruby
#
# Send firmware to a R2P2 device running dfutcp.
#
# Usage:
#   ruby send_firmware.rb <firmware_file> [options]
#
# Examples:
#   ruby send_firmware.rb app.rb
#   ruby send_firmware.rb app.rb --host 192.168.1.100
#   ruby send_firmware.rb app.mrb --host 192.168.1.100 --port 4649
#   ruby send_firmware.rb app.mrb --sign
#   ruby send_firmware.rb app.mrb --sign --key path/to/private.pem
#

require 'socket'
require 'zlib'
require 'optparse'

DEFAULT_KEY_PATH = File.expand_path("../../keys/dfu_ecdsa_private.pem", __FILE__)

options = {
  host: "localhost",
  port: 4649,
  sign: false,
  key_path: DEFAULT_KEY_PATH,
}

opt = OptionParser.new
opt.banner = "Usage: ruby #{$0} <firmware_file> [options]"
opt.on("--host HOST", "Target host (default: localhost)") { |v| options[:host] = v }
opt.on("--port PORT", Integer, "Target port (default: 4649)") { |v| options[:port] = v }
opt.on("--sign", "Sign firmware with ECDSA private key") { options[:sign] = true }
opt.on("--key PATH", "Path to ECDSA private key PEM (default: keys/dfu_ecdsa_private.pem)") do |v|
  options[:key_path] = v
  options[:sign] = true
end
opt.parse!(ARGV)

firmware_path = ARGV[0]

unless firmware_path
  $stderr.puts opt.banner
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

signature = nil
if options[:sign]
  require 'openssl'
  unless File.exist?(options[:key_path])
    $stderr.puts "Private key not found: #{options[:key_path]}"
    exit 1
  end
  private_key = OpenSSL::PKey::EC.new(File.read(options[:key_path]))
  signature   = private_key.sign("SHA256", firmware)
  puts "Signed with #{options[:key_path]} (#{signature.size} bytes)"
end

sig_len = signature ? signature.size : 0
header  = ["DFU\0", 1, type, firmware.size, crc32, sig_len].pack("a4Ca4NNn")

puts "Sending #{firmware_path} (#{type}, #{firmware.size} bytes, CRC32=0x#{crc32.to_s(16)})"
puts "Connecting to #{options[:host]}:#{options[:port]}..."

sock = TCPSocket.new(options[:host], options[:port])
# Flush IO-level buffer immediately on write so no bytes are held back.
sock.sync = true
# Disable Nagle to prevent ACK-Nagle deadlock: the last few bytes of a
# large payload can get stuck in the send buffer waiting for a delayed ACK
# from the receiver, which in turn waits for more data before sending ACK.
sock.setsockopt(Socket::IPPROTO_TCP, Socket::TCP_NODELAY, 1)

# Combine all data into one write to avoid per-write segmentation issues.
payload = header + (signature || "") + firmware
puts "[send] payload #{header.size}+#{sig_len}+#{firmware.size}=#{payload.size} bytes"
n = sock.write(payload)
puts "[send] write() returned #{n}"

puts "[send] waiting for response..."
response = sock.gets
sock.close

unless response
  $stderr.puts "No response from device"
  exit 1
end

puts response
exit 1 unless response.start_with?("OK")
